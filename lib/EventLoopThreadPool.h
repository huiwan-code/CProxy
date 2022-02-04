#pragma once
#include <memory>
#include <vector>
#include "EventLoop.h"
#include "EventLoopThread.h"

class EventLoopThreadPool {
  public:
    EventLoopThreadPool(int numThreads)
    :numThreads_(numThreads > 0 ? numThreads : 1), 
    nextWorkThreadIdx_(0){};
    ~EventLoopThreadPool(){};
    void start();
    SP_EventLoopThread pickRandThread();
  private:
    int numThreads_;
    int nextWorkThreadIdx_;
    std::vector<SP_EventLoopThread> threads_;
};

typedef std::shared_ptr<EventLoopThreadPool> SP_EventLoopThreadPool;