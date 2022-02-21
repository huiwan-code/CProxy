#include <string.h>
#include <netinet/in.h>

#include "Tunnel.h"
#include "Client.h"
#include "lib/Util.h"
#include "LocalConn.h"
#include "lib/ProxyConn.h"

std::string Tunnel::getValidProxyID() {
  std::string proxy_id = rand_str(5);
  while(proxy_conn_map.isExist(proxy_id)) {
    proxy_id = rand_str(5);
  }
  return proxy_id;
}

SP_ProxyConn Tunnel::createProxyConn(u_int32_t proxy_port) {
  // 连接服务端代理端口
  int proxy_conn_fd = tcp_connect((client_->proxy_server_host).c_str(), proxy_port);
  printf("client connect proxy ret fd: %d; port: %d\n", proxy_conn_fd, proxy_port);
  if (proxy_conn_fd <= 0) {
    printf("connect proxy port error\n");
    return SP_ProxyConn{};
  }

  SP_EventLoopThread threadPicked = work_pool_->pickRandThread();
  // 封装proxyConn
  SP_ProxyConn proxyConn(new ProxyConn(proxy_conn_fd, threadPicked->getLoop()));
  proxyConn->setStartProxyConnReqHandler_(std::bind(&Tunnel::handleStartProxyConnReq, this, std::placeholders::_1, std::placeholders::_2));
  threadPicked->addConn(proxyConn);
  
  std::string proxyID = getValidProxyID();
  proxyConn->setProxyID(proxyID);
  return proxyConn;
};

SP_LocalConn Tunnel::createLocalConn(std::string proxy_id) {
  int local_conn_fd = tcp_connect(local_server_host_.c_str(), local_server_port_);
  if (local_conn_fd <= 0) {
    printf("connect local server error\n");
    return SP_LocalConn{};
  }

  SP_EventLoopThread threadPicked = work_pool_->pickRandThread();
  // 封装localConn
  SP_LocalConn localConn(new LocalConn(local_conn_fd, threadPicked->getLoop(), this, proxy_id));
  threadPicked->addConn(localConn);
  return localConn;
};

void Tunnel::shutdownFromLocal(std::string proxy_id) {
  bool isProxyExist;
  SP_ProxyConn proxyConn = proxy_conn_map.get(proxy_id, isProxyExist);
  if (!isProxyExist) {
    printf("[shutdownFromLocal] proxy_id: %s not exist\n", proxy_id.c_str());
    return;
  }
  bool isFree = proxyConn->shutdownFromLocal();
  printf("[%s][shutdownFromLocal] proxy_id: %s isFree: %d\n", getNowTime(),proxy_id.c_str(), isFree);
  client_->shutdownFromLocal(tun_id_, proxy_id);
};

void Tunnel::handleStartProxyConnReq(void* msg, SP_ProxyConn conn) {
  StartProxyConnReqMsg *req_msg = (StartProxyConnReqMsg *)msg;
  std::string proxy_id = req_msg->proxy_id;
  u_int32_t public_fd = ntohl(req_msg->public_fd);
  printf("[%s][handleStartProxyConnReq] public_fd: %d; source_fd: %d\n", getNowTime(),public_fd, req_msg->public_fd);
  bool isProxyExist;
  SP_ProxyConn proxyConn = proxy_conn_map.get(proxy_id, isProxyExist);
  if (!isProxyExist) {
    printf("proxyConn %s not exist\n", proxy_id.c_str());
    return;
  }

  if (proxyConn->is_start()) {
    printf("proxyConn %s is starting, cannot start again\n", proxy_id.c_str());
    return;
  }

  // 创建localConn
  SP_LocalConn localConn = createLocalConn(proxy_id);
  // 将proxyConn设置为start
  proxyConn->start(localConn);
  
  StartProxyConnRspMsg rsp_msg;
  strcpy(rsp_msg.proxy_id, proxy_id.c_str());
  rsp_msg.public_fd = htonl(public_fd);
  printf("[handleStartProxyConnReq]public_fd2:%d\n", rsp_msg.public_fd);
  ProxyCtlMsg ctl_msg = make_proxy_ctl_msg(StartProxyConnRsp, (char *)&rsp_msg, sizeof(StartProxyConnRspMsg));
  proxyConn->send_msg(ctl_msg);
};