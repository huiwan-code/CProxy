#pragma once
#include <memory>

#include "channel.h"
#include "event_loop.h"

class Conn : public ChannelOwner {
 public:
  Conn(int fd, SP_EventLoop loop) : fd_(fd), loop_(loop), channel_(new Channel(fd)){};
  ~Conn() { loop_->RemoveFromPoller(channel_); }
  SP_Channel GetChannel() { return channel_; }
  int getFd() { return fd_; }

 protected:
  int fd_;
  SP_EventLoop loop_;
  SP_Channel channel_;
  bool in_buffer_empty_;
};

typedef std::shared_ptr<Conn> SP_Conn;