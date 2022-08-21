#include "event_loop.h"
#include "channel.h"
#include "epoll.h"
#include "util.h"

#include <sys/eventfd.h>
#include <mutex>
#include <vector>

EventLoop::EventLoop() : poller_(SP_Epoll(new Epoll())){};

void EventLoop::AddToPoller(SP_Channel channel) { poller_->PollAdd(channel); }

void EventLoop::UpdateToPoller(SP_Channel channel) { poller_->PollMod(channel); }

void EventLoop::RemoveFromPoller(SP_Channel channel) { poller_->PollDel(channel); }

void EventLoop::Loop() {
  std::vector<SP_Channel> ready_channels;
  for (;;) {
    ready_channels.clear();
    ready_channels = poller_->WaitForReadyChannels();
    for (SP_Channel chan : ready_channels) {
      chan->HandleEvents();
    }
  }
}
