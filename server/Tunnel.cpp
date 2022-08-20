#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <algorithm>

#include "control.h"
#include "public_conn.h"
#include "tunnel.h"
#include "lib/ctl_conn.h"
#include "lib/event_loop_thread.h"
#include "lib/proxy_conn.h"

void Tunnel::newPublicConnHandler() {
  struct sockaddr_in client_addr;
  memset(&client_addr, 0, sizeof(client_addr));
  socklen_t client_addr_len = sizeof(client_addr);
  int accept_fd = 0;
  while ((accept_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_addr_len)) > 0) {
    SPDLOG_INFO("public_accept_fd: {}; from:{}; port:{}", accept_fd,
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    bool freeIsEmpty;
    SP_ProxyConn proxyConn = popFreeProxyConn(freeIsEmpty);
    if (!freeIsEmpty) {
      reqStartProxy(accept_fd, proxyConn);
      // 将proxyConn暂挂到wait_for_start_proxy_conn_map_;
      wait_for_start_proxy_conn_map_.add(proxyConn->getProxyID(), proxyConn);
      continue;
    }

    {
      std::unique_lock<std::mutex> lock(public_fds_mutex_);
      undeal_public_fds_.push_back(accept_fd);
    }
    // 请求生成一个proxyConn
    ctl_->notifyClientNeedProxy(tun_id_);
  }
}

int Tunnel::getAndPopUndealPublicFd() {
  int fd;
  {
    std::unique_lock<std::mutex> lock(public_fds_mutex_);
    fd = undeal_public_fds_.back();
    undeal_public_fds_.pop_back();
  }
  return fd;
};

void Tunnel::claimProxyConn(SP_ProxyConn proxyConn) {
  proxyConn->setStartProxyConnRspHandler_(std::bind(&Tunnel::handleStartProxyConnRsp, this,
                                                    std::placeholders::_1, std::placeholders::_2));
  proxyConn->SetCloseHandler(
      std::bind(&Tunnel::handlePeerProxyConnClose, this, std::placeholders::_1));
  proxyConn->setCloseLocalPeerConnHandler_(
      std::bind(&Tunnel::shutdownPublicConn, this, std::placeholders::_1));
  int publicFd = getAndPopUndealPublicFd();
  bindPublicFdToProxyConn(publicFd, proxyConn);
};

void Tunnel::bindPublicFdToProxyConn(int publicFd, SP_ProxyConn proxyConn) {
  // 封装一个新publicConn
  SP_PublicConn publicConn(
      new PublicConn(publicFd, proxyConn->getThread(), this, proxyConn->getProxyID()));
  // 绑定proxyConn和public
  proxyConn->start(publicConn);
  proxy_conn_map_.add(proxyConn->getProxyID(), proxyConn);
};

void Tunnel::shutdownFromPublic(std::string proxy_id, u_int32_t tran_count) {
  bool isProxyExist;
  SP_ProxyConn proxyConn = proxy_conn_map_.get(proxy_id, isProxyExist);
  if (!isProxyExist) {
    return;
  }

  // 这里不能直接放到free列表中，避免proxyConn再次被选中时对端可能还处于isStart=true；
  proxyConn->shutdownFromLocal();
  ctl_->shutdownFromPublic(tun_id_, proxy_id, tran_count);
};

SP_ProxyConn Tunnel::popFreeProxyConn(bool& isEmpty) {
  std::unique_lock<std::mutex> lock(free_proxy_conns_mutex_);
  if (free_proxy_conns_.empty()) {
    isEmpty = true;
    return SP_ProxyConn{};
  }
  SP_ProxyConn proxyConn = free_proxy_conns_.back();
  free_proxy_conns_.pop_back();
  isEmpty = false;
  return proxyConn;
};

void Tunnel::reqStartProxy(int public_fd, SP_ProxyConn proxy_conn) {
  std::string proxy_id = proxy_conn->getProxyID();
  StartProxyConnReqMsg req_msg;
  strcpy(req_msg.proxy_id, proxy_id.c_str());
  req_msg.public_fd = htonl(public_fd);
  ProxyCtlMsg proxy_ctl_msg =
      make_proxy_ctl_msg(StartProxyConnReq, (char*)&req_msg, sizeof(StartProxyConnReqMsg));
  proxy_conn->SendMsg(proxy_ctl_msg);
}

void Tunnel::handleStartProxyConnRsp(void* msg, SP_ProxyConn proxyConn) {
  StartProxyConnRspMsg* rsp_msg = (StartProxyConnRspMsg*)msg;
  u_int32_t public_fd = ntohl(rsp_msg->public_fd);
  std::string proxy_id = proxyConn->getProxyID();
  if (!wait_for_start_proxy_conn_map_.isExist(proxy_id)) {
    SPDLOG_CRITICAL("proxy_id {} not exist in wait_for_start_proxy_conn_map", proxy_id);
    return;
  }
  bindPublicFdToProxyConn(public_fd, proxyConn);
  wait_for_start_proxy_conn_map_.erase(proxy_id);
};

void Tunnel::handlePeerProxyConnClose(SP_ProxyConn proxyConn) {
  std::string proxy_id = proxyConn->getProxyID();
  proxy_conn_map_.erase(proxy_id);
  {
    std::unique_lock<std::mutex> lock(free_proxy_conns_mutex_);
    std::remove(free_proxy_conns_.begin(), free_proxy_conns_.end(), proxyConn);
  }
}

void Tunnel::shutdownPublicConn(SP_ProxyConn proxyConn) {
  // 代理链接未把对端数据全部转发到publicConn上，还不能关闭
  int theoreticalTotalRecvCount = proxyConn->getTheoreticalTotalRecvCount();
  int recvCount = proxyConn->getRecvCount();
  if (recvCount != theoreticalTotalRecvCount) {
    return;
  }
  bool isFree = proxyConn->shutdownFromRemote();
  if (isFree) {
    freeProxyConn(proxyConn->getProxyID());
  }
};

void Tunnel::freeProxyConn(std::string proxy_id) {
  bool proxyConnIsExist;
  SP_ProxyConn proxyConn = proxy_conn_map_.get(proxy_id, proxyConnIsExist);
  if (!proxyConnIsExist) {
    SPDLOG_CRITICAL("proxy_id {} not exist", proxy_id);
    return;
  }
  if (proxyConn->is_start()) {
    SPDLOG_CRITICAL("proxy_id {} is starting", proxy_id);
    return;
  }
  proxy_conn_map_.erase(proxy_id);
  {
    std::unique_lock<std::mutex> lock(free_proxy_conns_mutex_);
    free_proxy_conns_.emplace_back(proxyConn);
  }
};

SP_ProxyConn Tunnel::getProxyConn(std::string proxy_id, bool& isExist) {
  return proxy_conn_map_.get(proxy_id, isExist);
}