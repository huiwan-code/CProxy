#pragma once
#include <sys/epoll.h>
#include <memory>
#include "lib/channel.h"
#include "lib/event_loop.h"
#include "lib/event_loop_thread.h"
#include "lib/tran_conn.h"
class Tunnel;
class PublicConn : public TranConn, public std::enable_shared_from_this<PublicConn> {
 public:
  PublicConn(int fd, SP_EventLoopThread thread, Tunnel* tun, std::string proxy_id)
      : TranConn(fd, thread), tun_(tun), proxy_id_(proxy_id), closing_(false) {
    channel_->SetEvents(EPOLLET | EPOLLIN | EPOLLRDHUP);
    channel_->SetReadHandler(std::bind(&PublicConn::handleRead, this));
    channel_->SetPostHandler(std::bind(&PublicConn::postHandle, this));
    channel_->SetNeedCloseWhenDelete(false);
  }
  ~PublicConn() { printf("publicConn killing\n"); }

 private:
  Tunnel* tun_;
  std::string proxy_id_;
  bool closing_;
  void handleRead();
  void postHandle();
};

using SP_PublicConn = std::shared_ptr<PublicConn>;