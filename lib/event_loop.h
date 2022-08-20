#pragma once
#include <memory>
#include <mutex>

#include "channel.h"
#include "event_dispatcher.h"
#include "util.h"

class EventLoop {
 public:
  EventLoop();
  void Loop();
  void AddToPoller(SP_Channel channel);
  void UpdateToPoller(SP_Channel channel);
  void RemoveFromPoller(SP_Channel channel);

 private:
  SP_EventDispatcher poller_;
};

typedef std::shared_ptr<EventLoop> SP_EventLoop;