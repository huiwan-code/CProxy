#include <assert.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <functional>
#include <iostream>

#include "client.h"
#include "local_conn.h"
#include "tunnel.h"
#include "lib/channel.h"
#include "lib/ctl_conn.h"
#include "lib/event_loop.h"
#include "lib/event_loop_thread_pool.h"
#include "lib/proxy_conn.h"
#include "lib/util.h"
#include "spdlog/spdlog.h"

Client::Client(int workThreadNum, std::string proxy_server_host, u_int32_t proxy_server_port,
               std::string local_server_host, u_int32_t local_server_port)
    : loop_(new EventLoop()),
      eventLoopThreadPool_(new EventLoopThreadPool(workThreadNum)),
      proxy_server_host_(proxy_server_host),
      proxy_server_port_(proxy_server_port),
      local_server_host_(local_server_host),
      local_server_port_(local_server_port) {
  ignoreSigpipe();
}

void Client::start() {
  eventLoopThreadPool_->start();
  initCtlConn();
  reqNewCtl();
  loop_->Loop();
}

// 因为一个客户端只会有一个ctlConn，所以在ctlConn中的处理函数都不需要加锁处理
void Client::initCtlConn() {
  int conn_fd = tcp_connect(proxy_server_host_.c_str(), proxy_server_port_);
  assert(conn_fd > 0);
  SP_CtlConn conn(new CtlConn(conn_fd, loop_));
  ctl_conn_ = conn;
  ctl_conn_->SetNewCtlRspHandler(
      std::bind(&Client::handleNewCtlRsp, this, std::placeholders::_1, std::placeholders::_2));
  ctl_conn_->SetCloseHandler(std::bind(&Client::handleCtlConnClose, this, std::placeholders::_1));
  ctl_conn_->SetNewTunnelRspHandler(
      std::bind(&Client::handleNewTunnelRsp, this, std::placeholders::_1, std::placeholders::_2));
  ctl_conn_->SetNotifyClientNeedProxyHandler(
      std::bind(&Client::handleProxyNotify, this, std::placeholders::_1, std::placeholders::_2));
  ctl_conn_->SetNotifyProxyShutdownPeerConnHandler(std::bind(
      &Client::handleShutdownLocalConn, this, std::placeholders::_1, std::placeholders::_2));
  loop_->AddToPoller(ctl_conn_->GetChannel());
}
void Client::handleNewCtlRsp(void *msg, SP_CtlConn conn) {
  NewCtlRspMsg *new_ctl_rsp_msg = (NewCtlRspMsg *)msg;
  conn->SetCtlId(std::string(new_ctl_rsp_msg->ctl_id));
  client_id_ = std::string(new_ctl_rsp_msg->ctl_id);
  reqNewTunnel();
}

void Client::handleNewTunnelRsp(void *msg, SP_CtlConn conn) {
  NewTunnelRspMsg *rsp_msg = (NewTunnelRspMsg *)msg;
  std::string tun_id = std::string(rsp_msg->tun_id);
  std::string local_server_host = std::string(rsp_msg->local_server_host);
  std::string proxy_server_host = std::string(rsp_msg->proxy_server_host);
  SP_Tunnel tun(new Tunnel{tun_id, local_server_host, rsp_msg->local_server_port, proxy_server_host,
                           rsp_msg->proxy_server_port, this, eventLoopThreadPool_});
  SPDLOG_INFO("tunnel addr:{}:{}", rsp_msg->proxy_server_host, rsp_msg->proxy_server_port);
  tunnel_map_.emplace(rsp_msg->tun_id, tun);
}

// 请求新ctl
void Client::reqNewCtl() {
  // 请求新ctl
  NewCtlReqMsg req_msg = NewCtlReqMsg{};
  CtlMsg msg = MakeCtlMsg(NewCtlReq, (char *)(&req_msg), sizeof(req_msg));
  ctl_conn_->SendMsg(msg);
};

// 新建应用隧道
void Client::reqNewTunnel() {
  NewTunnelReqMsg req_msg;
  req_msg.local_server_port = local_server_port_;
  strcpy(req_msg.local_server_host, local_server_host_.c_str());
  CtlMsg msg = MakeCtlMsg(NewTunnelReq, (char *)(&req_msg), sizeof(req_msg));
  ctl_conn_->SendMsg(msg);
}

// 处理服务端通知创建proxyConn
void Client::handleProxyNotify(void *msg, SP_CtlConn conn) {
  NotifyClientNeedProxyMsg *req_msg = (NotifyClientNeedProxyMsg *)msg;
  std::string tun_id = req_msg->tun_id;
  // 检查tun_id是否存在
  if (tunnel_map_.find(tun_id) == tunnel_map_.end()) {
    SPDLOG_CRITICAL("tun_id {} not exist", tun_id);
    return;
  }

  SP_Tunnel tun = tunnel_map_[tun_id];
  // 创建proxyConn
  SP_ProxyConn proxyConn = tun->createProxyConn(req_msg->server_proxy_port);
  (tun->proxy_conn_map).add(proxyConn->getProxyID(), proxyConn);

  // 创建LocalConn
  SP_LocalConn localConn = tun->createLocalConn(proxyConn);

  // 发送给服务端告知这个代理链接一些元信息
  ProxyMetaSetMsg meta_set_req_msg = ProxyMetaSetMsg{};
  strcpy(meta_set_req_msg.ctl_id, client_id_.c_str());
  strcpy(meta_set_req_msg.tun_id, tun_id.c_str());
  strcpy(meta_set_req_msg.proxy_id, (proxyConn->getProxyID()).c_str());
  ProxyCtlMsg proxy_ctl_msg =
      make_proxy_ctl_msg(ProxyMetaSet, (char *)&meta_set_req_msg, sizeof(meta_set_req_msg));
  if ((proxyConn->send_msg_dirct(proxy_ctl_msg)) == -1) {
    SPDLOG_CRITICAL("proxyConn {} send ProxyMetaSetMsg failed", proxyConn->getProxyID());
    return;
  };

  // 将proxyConn设置为start, 需在send_msg_dirct之后，避免localConn先发送数据到Server端
  proxyConn->start(localConn);
}

// 处理关闭localConn
void Client::handleShutdownLocalConn(void *msg, SP_CtlConn conn) {
  NotifyProxyShutdownPeerConnMsg *req_msg = (NotifyProxyShutdownPeerConnMsg *)msg;
  std::string tun_id = req_msg->tun_id;
  std::string proxy_id = req_msg->proxy_id;
  u_int32_t theoreticalTotalRecvCount = ntohl(req_msg->tran_count);
  // 检查tun_id是否存在
  if (tunnel_map_.find(tun_id) == tunnel_map_.end()) {
    SPDLOG_CRITICAL("tun_id {} not exist", tun_id);
    return;
  }
  SP_Tunnel tun = tunnel_map_[tun_id];
  bool isProxyExist;
  SP_ProxyConn proxyConn = (tun->proxy_conn_map).get(proxy_id, isProxyExist);
  if (!isProxyExist) {
    SPDLOG_CRITICAL("proxy_id: {} not exist", proxy_id);
    return;
  }
  proxyConn->incrTheoreticalTotalRecvCount(theoreticalTotalRecvCount);
  tun->shutdonwLocalConn(proxyConn);
};

void Client::shutdownFromLocal(std::string tun_id, std::string proxy_id, u_int32_t tran_count) {
  NotifyProxyShutdownPeerConnMsg req_msg;
  req_msg.tran_count = htonl(tran_count);
  strcpy(req_msg.tun_id, tun_id.c_str());
  strcpy(req_msg.proxy_id, proxy_id.c_str());
  CtlMsg ctl_msg = MakeCtlMsg(NotifyProxyShutdownPeerConn, (char *)&req_msg,
                                sizeof(NotifyProxyShutdownPeerConnMsg));
  ctl_conn_->SendMsg(ctl_msg);
};