#include "event_loop_thread_pool.h"
#include "event_loop_thread.h"

void EventLoopThreadPool::start() {
  for (int i = 0; i < thread_num_; i++) {
    SP_EventLoopThread t(new EventLoopThread());
    t->StartLoop();
    threads_.emplace_back(t);
  }
}

SP_EventLoopThread EventLoopThreadPool::PickRandThread() {
  SP_EventLoopThread t;
  {
    std::unique_lock<std::mutex> lock(thread_mutex_);
    t = threads_[next_work_thread_Idx_];
    next_work_thread_Idx_ = (next_work_thread_Idx_ + 1) % thread_num_;
  }
  return t;
}
