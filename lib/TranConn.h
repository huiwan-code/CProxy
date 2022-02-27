#pragma once
#include <unistd.h>

#include "Conn.h"
#include "EventLoop.h"
#include "EventLoopThread.h"
class TranConn : public Conn {
  public:
    TranConn(int fd, SP_EventLoopThread thread) 
    : Conn{fd, thread->getLoop()},
      thread_(thread),
      recv_count_mutex_(),
      tran_count_mutex_(),
      theoretical_total_recv_count_mutex_(),
      tran_count_(0),
      recv_count_(0),
      theoretical_total_recv_count_(-1) {
      pipe(pipe_fds_);
    }
    ~TranConn(){
      close(pipe_fds_[0]);
      close(pipe_fds_[1]);
    }
    void setPeerConnFd(int fd) {peer_conn_fd_ = fd;}
    SP_EventLoopThread getThread(){return thread_;}

    u_int32_t getRecvCount() {
      std::unique_lock<std::mutex> lock(recv_count_mutex_);
      return recv_count_;
    };
    void incrRecvCount(u_int32_t addedCount) {
      std::unique_lock<std::mutex> lock(recv_count_mutex_);
      recv_count_ += addedCount;
    }

    void resetRecvCount() {
      std::unique_lock<std::mutex> lock(recv_count_mutex_);
      recv_count_ = 0;
    }
    u_int32_t getTranCount() {
      std::unique_lock<std::mutex> lock(tran_count_mutex_);
      return tran_count_;
    }

    void incrTranCount(u_int32_t addedCount) {
      std::unique_lock<std::mutex> lock(tran_count_mutex_);
      tran_count_ += addedCount;
    }

    void resetTranCount() {
      std::unique_lock<std::mutex> lock(tran_count_mutex_);
      tran_count_ = 0;
    }
    int getTheoreticalTotalRecvCount() {
      std::unique_lock<std::mutex> lock(theoretical_total_recv_count_mutex_);
      return theoretical_total_recv_count_;
    }

    void incrTheoreticalTotalRecvCount(u_int32_t addedCount) {
      std::unique_lock<std::mutex> lock(theoretical_total_recv_count_mutex_);
      if (theoretical_total_recv_count_ == -1) {
        theoretical_total_recv_count_ = addedCount;
      } else {
        theoretical_total_recv_count_ += addedCount;
      }
      
    }

    void resetTheoreticalTotalRecvCount() {
      std::unique_lock<std::mutex> lock(theoretical_total_recv_count_mutex_);
      theoretical_total_recv_count_ = -1;
    }

    void resetPipeFd() {
      close(pipe_fds_[0]);
      close(pipe_fds_[1]);
      pipe(pipe_fds_);
    }
  protected:
    // 数据流向对端的管道
    int pipe_fds_[2];
    int peer_conn_fd_;
    SP_EventLoopThread thread_;
    std::mutex recv_count_mutex_;
    std::mutex tran_count_mutex_;
    std::mutex theoretical_total_recv_count_mutex_;
    
    u_int32_t recv_count_;
    u_int32_t tran_count_;
    // 初始化为-1，
    int theoretical_total_recv_count_;
};

using SP_TranConn = std::shared_ptr<TranConn>;