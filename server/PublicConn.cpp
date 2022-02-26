#include<unistd.h>
#include <fcntl.h>
#include <string.h>
#include "PublicConn.h"
#include "Tunnel.h"
#include "spdlog/spdlog.h"

void PublicConn::handleRead() {
  try{
    SPDLOG_INFO("get public read");
    int bs = splice(fd_, NULL, pipe_fds_[1], NULL, 2048, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
    if (bs < 0) {
        SPDLOG_CRITICAL("public_fd: {} -> pipe_fd: {} splice err: {}", fd_, pipe_fds_[1], strerror(errno));
        return;
    }
    if (bs == 0) {
      // 收到fin包,通知proxy
      SPDLOG_INFO("public_fd: {} proxy_id: {} get fin", fd_, proxy_id_);
      tun_->shutdownFromPublic(proxy_id_, getTranCount());
      SPDLOG_INFO("after shutdownFromPublic0");
      closing_ = true;
      SPDLOG_INFO("after shutdownFromPublic1");
      return;
    }
    bs = splice(pipe_fds_[0], NULL, peer_conn_fd_, NULL, bs, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
    if (bs < 0) {
        SPDLOG_CRITICAL("proxy_id {} pipe {} - >proxy_fd: {} splice err: {}", proxy_id_, pipe_fds_[0], peer_conn_fd_, strerror(errno));
        return;
    }
    incrTranCount(bs);
  } catch (const std::exception& e) {
    SPDLOG_CRITICAL("read public except: {}", e.what());
  }
}

void PublicConn::postHandle() {
  SPDLOG_INFO("PublicConn::postHandle");
  if (closing_) {
    SPDLOG_INFO("PublicConn::postHandle closing");
    return;
  }
  channel_->setEvents(EPOLLET | EPOLLIN  | EPOLLRDHUP);
  loop_->updatePoller(channel_);
}