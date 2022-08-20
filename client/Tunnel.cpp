#include <netinet/in.h>
#include <string.h>

#include "client.h"
#include "local_conn.h"
#include "tunnel.h"
#include "lib/proxy_conn.h"
#include "lib/util.h"
#include "spdlog/spdlog.h"

std::string Tunnel::getValidProxyID() {
  std::string proxy_id = rand_str(5);
  while (proxy_conn_map.isExist(proxy_id)) {
    proxy_id = rand_str(5);
  }
  return proxy_id;
}

SP_ProxyConn Tunnel::createProxyConn(u_int32_t proxy_port) {
  // 连接服务端代理端口
  int proxy_conn_fd = tcp_connect((client_->proxy_server_host_).c_str(), proxy_port);
  if (proxy_conn_fd <= 0) {
    SPDLOG_CRITICAL("connect proxy port error");
    return SP_ProxyConn{};
  }

  SP_EventLoopThread threadPicked = work_pool_->PickRandThread();
  // 封装proxyConn
  SP_ProxyConn proxyConn(new ProxyConn(proxy_conn_fd, threadPicked));
  proxyConn->setStartProxyConnReqHandler_(std::bind(&Tunnel::handleStartProxyConnReq, this,
                                                    std::placeholders::_1, std::placeholders::_2));
  proxyConn->setCloseLocalPeerConnHandler_(
      std::bind(&Tunnel::shutdonwLocalConn, this, std::placeholders::_1));
  threadPicked->AddConn(proxyConn);

  std::string proxyID = getValidProxyID();
  proxyConn->setProxyID(proxyID);
  return proxyConn;
};

SP_LocalConn Tunnel::createLocalConn(SP_ProxyConn proxyConn) {
  int local_conn_fd = tcp_connect(local_server_host_.c_str(), local_server_port_);
  if (local_conn_fd <= 0) {
    SPDLOG_CRITICAL("connect local server error");
    return SP_LocalConn{};
  }
  // 封装localConn
  SP_LocalConn localConn(
      new LocalConn(local_conn_fd, proxyConn->getThread(), this, proxyConn->getProxyID()));
  return localConn;
};

void Tunnel::shutdownFromLocal(std::string proxy_id, u_int32_t tran_count) {
  bool isProxyExist;
  SP_ProxyConn proxyConn = proxy_conn_map.get(proxy_id, isProxyExist);
  if (!isProxyExist) {
    SPDLOG_CRITICAL("proxy_id: {} not exist", proxy_id);
    return;
  }
  proxyConn->shutdownFromLocal();
  client_->shutdownFromLocal(tun_id_, proxy_id, tran_count);
};

void Tunnel::handleStartProxyConnReq(void *msg, SP_ProxyConn conn) {
  StartProxyConnReqMsg *req_msg = (StartProxyConnReqMsg *)msg;
  std::string proxy_id = req_msg->proxy_id;
  u_int32_t public_fd = ntohl(req_msg->public_fd);
  bool isProxyExist;
  SP_ProxyConn proxyConn = proxy_conn_map.get(proxy_id, isProxyExist);
  if (!isProxyExist) {
    SPDLOG_CRITICAL("proxyConn {} not exist", proxy_id);
    return;
  }

  if (proxyConn->is_start()) {
    SPDLOG_CRITICAL("proxyConn {} is starting, cannot start again", proxy_id);
    return;
  }

  // 创建localConn
  SP_LocalConn localConn = createLocalConn(proxyConn);

  StartProxyConnRspMsg rsp_msg;
  strcpy(rsp_msg.proxy_id, proxy_id.c_str());
  rsp_msg.public_fd = htonl(public_fd);
  ProxyCtlMsg ctl_msg =
      make_proxy_ctl_msg(StartProxyConnRsp, (char *)&rsp_msg, sizeof(StartProxyConnRspMsg));
  if ((proxyConn->send_msg_dirct(ctl_msg)) == -1) {
    SPDLOG_CRITICAL("proxyConn {} send StartProxyConnRspMsg fail", proxy_id);
    return;
  };

  // 将proxyConn设置为start, 需在send_msg_dirct之后，避免localConn先发送数据到Server端
  proxyConn->start(localConn);
};

void Tunnel::shutdonwLocalConn(SP_ProxyConn proxyConn) {
  // 当前proxyConn还未把对端的数据全部传到LocalConn缓冲区上
  if (proxyConn->getTheoreticalTotalRecvCount() != proxyConn->getRecvCount()) {
    return;
  }
  bool isFree = proxyConn->shutdownFromRemote();
  // 如果本端代理连接已经空闲，需要通知将此代理释放到空闲列表中
  if (isFree) {
    FreeProxyConnReqMsg req_msg;
    strcpy(req_msg.tun_id, tun_id_.c_str());
    strcpy(req_msg.proxy_id, (proxyConn->getProxyID()).c_str());
    CtlMsg ctl_msg = MakeCtlMsg(FreeProxyConnReq, (char *)&req_msg, sizeof(FreeProxyConnReqMsg));
    (client_->getCtlConn())->SendMsg(ctl_msg);
  }
};