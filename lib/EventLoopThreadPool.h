#pragma once
#include <memory>
#include <vector>
#include <mutex>
#include "EventLoop.h"
#include "EventLoopThread.h"

class EventLoopThreadPool {
  public:
    EventLoopThreadPool(int numThreads)
    :thread_mutex_(),
    numThreads_(numThreads > 0 ? numThreads : 1), 
    nextWorkThreadIdx_(0){};
    ~EventLoopThreadPool(){};
    void start();
    SP_EventLoopThread pickRandThread();
  private:
    std::mutex thread_mutex_;
    int numThreads_;
    int nextWorkThreadIdx_;
    std::vector<SP_EventLoopThread> threads_;
};

typedef std::shared_ptr<EventLoopThreadPool> SP_EventLoopThreadPool;