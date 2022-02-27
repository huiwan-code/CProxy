#include <fcntl.h>
#include <string.h>
#include "LocalConn.h"
#include "Tunnel.h"
#include "lib/Util.h"

void LocalConn::handleRead() {
  try{
    SPDLOG_INFO("localConn: fd: {} flag: {}", fd_, flag);
    int bs = splice(fd_, NULL, pipe_fds_[1], NULL, 2048, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
    if (bs < 0) {
        SPDLOG_CRITICAL("proxy_id: {} flag: {} local_fd: {} -> pipe_fd: {} splice err: {} ", proxy_id_, flag, fd_, pipe_fds_[1], strerror(errno));
        return;
    }
    // 当proxyConn调用shutdownFromRemote后，shutdown(local_fd, 1)后会往local_fd中添加EPOLLIN事件，所以可能会重复满足bs=0，需要添加closing_判断。避免多次进入循环
    // https://mp.weixin.qq.com/s?__biz=MzUxNDUwOTc0Nw==&mid=2247484253&idx=1&sn=e42ed5e1af8382eb6dffa7550470cdf3
    if (bs == 0 && !closing_) {
      SPDLOG_INFO("proxy_id: {} local_fd: {} proxy_id: {} get fin", proxy_id_, fd_, proxy_id_);
      tun_->shutdownFromLocal(proxy_id_, getTranCount());
      closing_ = true;
      return;
    }
    bs = splice(pipe_fds_[0], NULL, peer_conn_fd_, NULL, bs, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
    if (bs < 0) {
        SPDLOG_CRITICAL("proxy_id {} local_fd: {} pipe_fd: {} -> proxy_conn_fd: {} splice err: {}", proxy_id_, fd_, pipe_fds_[0], peer_conn_fd_, strerror(errno));
        return;
    }
    incrTranCount(bs);
    SPDLOG_INFO("proxy {} local_fd {} tran count {}", proxy_id_, fd_, bs);
  } catch (const std::exception& e) {
    SPDLOG_CRITICAL("read local_conn except: {}", e.what());
  }
}

void LocalConn::postHandle() {
  if (closing_) {
    return;
  }
  channel_->setEvents(EPOLLET | EPOLLIN  | EPOLLRDHUP);
  loop_->updatePoller(channel_);
}