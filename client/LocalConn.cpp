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
    if (bs == 0) {
      SPDLOG_INFO("proxy_id: {} local_fd: {} proxy_id: {} get fin", proxy_id_, fd_, proxy_id_);
      tun_->shutdownFromLocal(proxy_id_, getTranCount());
      closing_ = true;
      SPDLOG_INFO("after shutdownFromLocal");
      return;
    }
    bs = splice(pipe_fds_[0], NULL, peer_conn_fd_, NULL, bs, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
    if (bs < 0) {
        SPDLOG_CRITICAL("proxy_id {} local_fd: {} pipe_fd: {} -> proxy_conn_fd: {} splice err: {}", proxy_id_, fd_, pipe_fds_[0], peer_conn_fd_, strerror(errno));
        return;
    }
    incrTranCount(bs);
    SPDLOG_INFO("proxy_id {} local_fd: {} sent {}", proxy_id_, fd_, getTranCount());
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