#pragma once
#include <unistd.h>

#include "Conn.h"
#include "EventLoop.h"

class TranConn : public Conn {
  public:
    TranConn(int fd, SP_EventLoop loop) : Conn{fd, loop} {
      pipe(pipe_fds_);
    }
    void setPeerConnFd(int fd) {peer_conn_fd_ = fd;}
  protected:
    // 数据流向对端的管道
    int pipe_fds_[2];
    int peer_conn_fd_;
};

using SP_TranConn = std::shared_ptr<TranConn>;