#pragma once
#include <memory>
#include <mutex>
#include <vector>
#include "event_loop.h"
#include "event_loop_thread.h"

class EventLoopThreadPool {
 public:
  EventLoopThreadPool(int thread_num)
      : thread_mutex_(), thread_num_(thread_num > 0 ? thread_num : 1), next_work_thread_Idx_(0){};
  ~EventLoopThreadPool(){};
  void start();
  SP_EventLoopThread PickRandThread();

 private:
  std::mutex thread_mutex_;
  int thread_num_;
  int next_work_thread_Idx_;
  std::vector<SP_EventLoopThread> threads_;
};

typedef std::shared_ptr<EventLoopThreadPool> SP_EventLoopThreadPool;