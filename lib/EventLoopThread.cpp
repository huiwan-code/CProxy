#include <assert.h>
#include <thread>
#include <mutex>
#include <exception>
#include <iostream>

#include "Conn.h"
#include "EventLoopThread.h"
EventLoopThread::~EventLoopThread() {
  // 等待线程退出
  if (started_) {
    thread_.join();
  }
}
void EventLoopThread::threadFunc() 
try{
  if(!loop_) {
    throw "loop_ is null";
  }

  {
    std::unique_lock<std::mutex> lock(mutex_);
    started_ = true;
    cond_.notify_all();
  }
  loop_->loop();
}
catch(std::exception& e) {
  SPDLOG_CRITICAL("EventLoopThread::threadFunc exception: {}", e.what());
  abort();
}

void EventLoopThread::startLoop() {
  std::unique_lock<std::mutex> lock(mutex_);
  while(!started_) cond_.wait(lock);
}

void EventLoopThread::addChannel(SP_Channel chan) {
  loop_->addToPoller(chan);
}

void EventLoopThread::addConn(SP_Conn conn) {
  loop_->addToPoller(conn->getChannel());
}
