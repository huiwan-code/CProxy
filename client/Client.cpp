#include <assert.h>
#include <sys/epoll.h>
#include <functional>
#include <iostream>
#include <string.h>
#include "Client.h"
#include "lib/Util.h"
#include "lib/EventLoop.h"
#include "lib/EventLoopThreadPool.h"
#include "lib/Channel.h"
#include "lib/CtlConn.h"
#include "lib/ProxyConn.h"
#include "lib/LocalConn.h"

Client::Client(int workThreadNum) 
: loop_(new EventLoop()),
eventLoopThreadPool_(new EventLoopThreadPool(workThreadNum)){
  ignoreSigpipe();
}

void Client::start() {
  eventLoopThreadPool_->start();
  initCtlConn();
  reqNewCtl();
  loop_->loop();
}

void Client::initCtlConn() {
  int conn_fd = tcp_connect("127.0.0.1", 8080);
  assert(conn_fd > 0);
  SP_CtlConn conn(new CtlConn(conn_fd, loop_));
  ctl_conn_ = conn;
  ctl_conn_->setNewCtlRspHandler(
    std::bind(&Client::handleNewCtlRsp, this, std::placeholders::_1, std::placeholders::_2));
  ctl_conn_->setCloseHandler_(std::bind(&Client::handleCtlConnClose, this, std::placeholders::_1));
  ctl_conn_->setNewTunnelRspHandler(std::bind(&Client::handleNewTunnelRsp, this, std::placeholders::_1, std::placeholders::_2));
  ctl_conn_->setNotifyClientNeedProxyHandler(std::bind(&Client::handleProxyNotify, this, std::placeholders::_1, std::placeholders::_2));
  loop_->addToPoller(ctl_conn_->getChannel());
}
void Client::handleNewCtlRsp(void* msg, SP_CtlConn conn) {
  NewCtlRspMsg *new_ctl_rsp_msg = (NewCtlRspMsg *)msg;
  conn->set_ctl_id(std::string(new_ctl_rsp_msg->ctl_id));
  client_id_ = std::string(new_ctl_rsp_msg->ctl_id);
  reqNewTunnel();
}

void Client::handleNewTunnelRsp(void *msg, SP_CtlConn conn) {
  NewTunnelRspMsg *new_tunnel_rsp_msg = (NewTunnelRspMsg *)msg;
  SP_Tunnel tun(new Tunnel{});
  tun->tun_id = std::string(new_tunnel_rsp_msg->tun_id);
  std::cout << "client: " << tun->tun_id << std::endl;
  tun->server_port = new_tunnel_rsp_msg->server_port;
  tunnel_map_.emplace(tun->tun_id, tun);
}

// 请求新ctl
void Client::reqNewCtl() {
  // 请求新ctl
  NewCtlReqMsg req_msg = NewCtlReqMsg{};
  CtlMsg msg = make_ctl_msg(NewCtlReq, (char *)(&req_msg), sizeof(req_msg));
  ctl_conn_->send_msg(msg);
};

// 新建应用隧道
void Client::reqNewTunnel() {
  NewTunnelReqMsg req_msg = NewTunnelReqMsg{7777};
  CtlMsg msg = make_ctl_msg(NewTunnelReq, (char *)(&req_msg), sizeof(req_msg));
  ctl_conn_->send_msg(msg);
}

// 处理服务端通知创建proxyConn
void Client::handleProxyNotify(void *msg, SP_CtlConn conn) {
  NotifyClientNeedProxyMsg *req_msg = (NotifyClientNeedProxyMsg*)msg;
  std::string tun_id = req_msg->tun_id;
  // 检查tun_id是否存在
  if (tunnel_map_.find(tun_id) == tunnel_map_.end()) {
    printf("tun_id %s not exist\n", tun_id.c_str());
    return;
  }

  SP_Tunnel tun = tunnel_map_[tun_id];
  // 连接本地服务
  int local_conn_fd = tcp_connect("127.0.0.1", tun->server_port);
  if (local_conn_fd <= 0) {
    printf("connect local server error\n");
    return;
  }
  // 封装localConn
  SP_EventLoopThread threadPicked = eventLoopThreadPool_->pickRandThread();
  SP_LocalConn localConn(new LocalConn(local_conn_fd, threadPicked->getLoop()));
  threadPicked->addConn(localConn);

  // 连接服务端代理端口
  int proxy_conn_fd = tcp_connect("127.0.0.1", req_msg->server_proxy_port);
  printf("client connect proxy ret fd: %d; port: %d\n", proxy_conn_fd, req_msg->server_proxy_port);
  if (proxy_conn_fd <= 0) {
    printf("connect proxy port error\n");
    return;
  }

  // 封装proxyConn
  SP_ProxyConn proxyConn(new ProxyConn(proxy_conn_fd, threadPicked->getLoop()));
  threadPicked->addConn(proxyConn);
  
  // proxy设置为开始
  proxyConn->start(localConn);

  // 发送给服务端告知这个代理链接一些元信息
  ProxyMetaSetMsg meta_set_req_msg = ProxyMetaSetMsg{};
  strcpy(meta_set_req_msg.ctl_id, client_id_.c_str());
  strcpy(meta_set_req_msg.tun_id, req_msg->tun_id);
  ProxyCtlMsg proxy_ctl_msg = make_proxy_ctl_msg(ProxyMetaSet, (char *)&meta_set_req_msg, sizeof(meta_set_req_msg));
  proxyConn->send_msg(proxy_ctl_msg);
  
  // 将proxyConn添加到tunnel中
  {
    std::unique_lock<std::mutex>lock(tun->mutex);
    (tun->proxy_conn_map).emplace(proxy_conn_fd, proxyConn);
  }
}