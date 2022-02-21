#pragma once
#include <memory>
#include <sys/epoll.h>
#include "lib/Channel.h"
#include "lib/EventLoop.h"
#include "lib/TranConn.h"

class Tunnel;
class PublicConn : public TranConn, public std::enable_shared_from_this<PublicConn>  {
  public:
    PublicConn(int fd, SP_EventLoop loop, Tunnel* tun, std::string proxy_id) 
    : TranConn(fd, loop),
      tun_(tun),
      proxy_id_(proxy_id),
      closing_(false) {
      channel_->setEvents(EPOLLET | EPOLLIN | EPOLLRDHUP);
      channel_->setReadHandler(std::bind(&PublicConn::handleRead, this));
      channel_->setPostHandler(std::bind(&PublicConn::postHandle, this));
    }
    ~PublicConn(){
      printf("publicConn killing\n");
    }
  private:
    Tunnel* tun_;
    std::string proxy_id_;
    bool closing_;
    void handleRead();
    void postHandle();
};

using SP_PublicConn = std::shared_ptr<PublicConn>;