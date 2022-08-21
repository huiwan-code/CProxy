#pragma once

#include <sys/syscall.h>
#include <unistd.h>
#include <memory>
#include <sys/epoll.h>
#include "lib/event_loop.h"
#include "lib/event_loop_thread.h"
#include "lib/tran_conn.h"
class Tunnel;
class LocalConn : public TranConn, public std::enable_shared_from_this<LocalConn> {
 public:
  LocalConn(int fd, SP_EventLoopThread thread, Tunnel* tun, std::string proxy_id)
      : TranConn(fd, thread), tun_(tun), proxy_id_(proxy_id), closing_(false) {
    channel_->SetEvents(EPOLLET | EPOLLIN | EPOLLRDHUP);
    channel_->SetReadHandler(std::bind(&LocalConn::handleRead, this));
    channel_->SetPostHandler(std::bind(&LocalConn::postHandle, this));
    channel_->SetNeedCloseWhenDelete(false);
  }
  ~LocalConn() { printf("local killing\n"); }

 private:
  Tunnel* tun_;
  std::string proxy_id_;
  bool closing_;
  void handleRead();
  void postHandle();
};

using SP_LocalConn = std::shared_ptr<LocalConn>;