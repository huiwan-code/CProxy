#pragma once

#include <memory>

#include "TranConn.h"
#include "EventLoop.h"

class LocalConn : public TranConn, public std::enable_shared_from_this<LocalConn>{
  public:
    LocalConn(int fd, SP_EventLoop loop): TranConn(fd, loop) {
      channel_->setEvents(EPOLLET | EPOLLIN | EPOLLRDHUP);
      channel_->setReadHandler(std::bind(&LocalConn::handleRead, this));
      channel_->setPostHandler(std::bind(&LocalConn::postHandle, this));
    }

  private:
    void handleRead();
    void postHandle();
};

using SP_LocalConn = std::shared_ptr<LocalConn>;