#pragma once
#include <memory>
#include <mutex>

#include "Epoll.h"
#include "Channel.h"
#include "Util.h"

class EventLoop {
  public:
    EventLoop();
    void loop();
    void addToPoller(SP_Channel channel);
    void updatePoller(SP_Channel channel);
    void removeFromPoller(SP_Channel channel);
  private:
    SP_Epoll poller_;
};

typedef std::shared_ptr<EventLoop> SP_EventLoop;