#pragma once
#include <memory>
#include <mutex>

#include "channel.h"
#include "epoll.h"
#include "util.h"

class EventLoop {
 public:
  EventLoop();
  void Loop();
  void AddToPoller(SP_Channel channel);
  void UpdateToPoller(SP_Channel channel);
  void RemoveFromPoller(SP_Channel channel);

 private:
  SP_Epoll poller_;
};

typedef std::shared_ptr<EventLoop> SP_EventLoop;