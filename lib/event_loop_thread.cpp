#include <assert.h>
#include <exception>
#include <iostream>
#include <mutex>
#include <thread>

#include "conn.h"
#include "event_loop_thread.h"
EventLoopThread::~EventLoopThread() {
  // 等待线程退出
  if (started_) {
    thread_.join();
  }
}
void EventLoopThread::ThreadFunc() try {
  if (!loop_) {
    throw "loop_ is null";
  }

  {
    std::unique_lock<std::mutex> lock(mutex_);
    started_ = true;
    cond_.notify_all();
  }
  loop_->Loop();
} catch (std::exception& e) {
  SPDLOG_CRITICAL("EventLoopThread::ThreadFunc exception: {}", e.what());
  abort();
}

void EventLoopThread::StartLoop() {
  std::unique_lock<std::mutex> lock(mutex_);
  while (!started_) cond_.wait(lock);
}

void EventLoopThread::AddChannel(SP_Channel chan) { loop_->AddToPoller(chan); }

void EventLoopThread::AddConn(SP_Conn conn) { loop_->AddToPoller(conn->GetChannel()); }
