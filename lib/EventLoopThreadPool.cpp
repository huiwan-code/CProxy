#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"

void EventLoopThreadPool::start() {
  for (int i = 0; i < numThreads_; i++) {
    SP_EventLoopThread t(new EventLoopThread());
    t->startLoop();
    threads_.emplace_back(t);
  }
}

SP_EventLoopThread EventLoopThreadPool::pickRandThread() {
  SP_EventLoopThread t = threads_[nextWorkThreadIdx_];
  nextWorkThreadIdx_ = (nextWorkThreadIdx_ + 1) % numThreads_;
  return t;
}

