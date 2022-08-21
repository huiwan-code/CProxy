#pragma once
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include "conn.h"
#include "event_loop.h"

class EventLoopThread {
 public:
  EventLoopThread()
      : thread_(std::bind(&EventLoopThread::ThreadFunc, this)),
        mutex_(),
        cond_(){};
  ~EventLoopThread();
  void StartLoop();
  void ThreadFunc();
  void AddChannel(SP_Channel);
  void AddConn(SP_Conn);
  SP_EventLoop GetLoop() { return loop_; }

 private:
  SP_EventLoop loop_;
  bool started_;
  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable cond_;
};

using SP_EventLoopThread = std::shared_ptr<EventLoopThread>;