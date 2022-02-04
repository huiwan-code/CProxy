#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#include "Tunnel.h"
#include "lib/EventLoopThread.h"
#include "lib/PublicConn.h"
#include "lib/CtlConn.h"
#include "Control.h"

void Tunnel::newPublicConnHandler() {
  struct sockaddr_in client_addr;
  memset(&client_addr, 0, sizeof(client_addr));
  socklen_t client_addr_len = sizeof(client_addr);
  int accept_fd = 0;
  while((accept_fd = accept(listen_fd_, (struct sockaddr *)&client_addr, &client_addr_len)) > 0) {
    printf("public_accept_fd: %d; from:%s; port:%d \n", accept_fd, inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
    // 获取一个空闲的proxyConn，没有的话把fd加入到未处理列表
    {
      std::unique_lock<std::mutex> lock(mutex_);
      undeal_public_fds_.push_back(accept_fd);
      // 请求生成一个proxyConn
      ctl_->notifyClientNeedProxy(tun_id_);
    }
  }
}

int Tunnel::getAndPopUndealPublicFd() {
  int fd;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    fd = undeal_public_fds_.back();
    undeal_public_fds_.pop_back();
  }
  return fd;
};

void Tunnel::claimProxyConn(SP_ProxyConn proxyConn) {
  int publicFd = getAndPopUndealPublicFd();
  // 选择一个工作线程处理
  SP_EventLoopThread threadPicked = work_pool_->pickRandThread();
  // 封装一个新publicConn
  SP_PublicConn publicConn(new PublicConn(publicFd, threadPicked->getLoop()));
  // 绑定proxyConn和public
  proxyConn->start(publicConn);
  // 将publicConn和工作线程关联
  threadPicked->addConn(publicConn);

  proxy_conn_map_.emplace(proxyConn->getFd(), proxyConn);
};