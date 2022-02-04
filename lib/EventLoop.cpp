#include "EventLoop.h"
#include "Epoll.h"
#include "Channel.h"
#include <vector>
void EventLoop::addToPoller(SP_Channel channel) {
  poller_->epoll_add(channel);
}

void EventLoop::updatePoller(SP_Channel channel) {
  poller_->epoll_mod(channel);
}

void EventLoop::removeFromPoller(SP_Channel channel) {
  poller_->epoll_del(channel);
}

void EventLoop::loop() {
  std::vector<SP_Channel> readyChannels;
  for(;;) {
    readyChannels.clear();
    readyChannels = poller_->waitForReadyChannels();
    for (SP_Channel chan:readyChannels) chan->handleEvents();
  }
}

