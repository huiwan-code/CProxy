#pragma once
#include <memory>
#include <sys/epoll.h>
#include "Channel.h"
#include "EventLoop.h"
#include "TranConn.h"


class PublicConn : public TranConn, public std::enable_shared_from_this<PublicConn>  {
  public:
    PublicConn(int fd, SP_EventLoop loop) : TranConn(fd, loop) {
      channel_->setEvents(EPOLLET | EPOLLIN | EPOLLRDHUP);
      channel_->setReadHandler(std::bind(&PublicConn::handleRead, this));
      channel_->setPostHandler(std::bind(&PublicConn::postHandle, this));
    }
    ~PublicConn(){
      printf("publicConn killing\n");
    }
  private:
    void handleRead();
    void postHandle();
};

using SP_PublicConn = std::shared_ptr<PublicConn>;