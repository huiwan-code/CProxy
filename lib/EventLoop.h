#pragma once
#include <memory>

#include "Epoll.h"
#include "Channel.h"

class EventLoop {
  public:
    EventLoop():poller_(new Epoll()){};
    void loop();
    void addToPoller(SP_Channel channel);
    void updatePoller(SP_Channel channel);
    void removeFromPoller(SP_Channel channel);
  private:
    SP_Epoll poller_;
};

typedef std::shared_ptr<EventLoop> SP_EventLoop;