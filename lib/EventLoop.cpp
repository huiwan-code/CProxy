#include "EventLoop.h"
#include "Epoll.h"
#include "Channel.h"
#include "Util.h"

#include <mutex>
#include <vector>
#include <sys/eventfd.h>

EventLoop::EventLoop()
  :poller_(new Epoll()),
  // https://zhuanlan.zhihu.com/p/40572954
  wakeupFd_(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)),
  mutex_(),
  wakeupChannel_(new Channel(wakeupFd_)){
    wakeupChannel_->setEvents(EPOLLIN | EPOLLET);
    wakeupChannel_->setReadHandler(std::bind(&EventLoop::wakeupChannelReadHandler, this));
    wakeupChannel_->setPostHandler(std::bind(&EventLoop::wakeupChannelPostHandler, this));
  };

void EventLoop::wakeupChannelReadHandler() {
  uint32_t msg = 1;
  bool tmp;
  size_t n = readn(wakeupFd_, (char *)&msg, msg, tmp);
};

void EventLoop::wakeupChannelPostHandler() {
  wakeupChannel_->setEvents(EPOLLIN | EPOLLET);
  updatePoller(wakeupChannel_);
};

// 来自不同线程调用，需要对pendingFunctors队列加锁
void EventLoop::addPendingFunctor(voidFunctor&& functor) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    pendingFunctors_.emplace_back(std::move(functor));
  }

  // 往wakeupFd_写入数据，触发epoll
  uint32_t msg = 1;
  size_t n = writen(wakeupFd_, (char *)&msg, msg);
};

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
    // 检查是否有待处理的函数
    if (!pendingFunctors_.empty()) {
      std::vector<voidFunctor> voidFunctors;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        voidFunctors.swap(pendingFunctors_);
      }
      for (voidFunctor fn : voidFunctors) fn();
    }
  }
}

