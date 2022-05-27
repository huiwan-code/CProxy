#pragma once
#include <sys/epoll.h>
#include <memory>
#include <vector>
#include "Channel.h"

class Epoll {
 public:
  Epoll();
  ~Epoll(){};
  void epoll_add(SP_Channel);
  void epoll_mod(SP_Channel);
  void epoll_del(SP_Channel);
  std::vector<SP_Channel> waitForReadyChannels();
  int get_fd() { return epoll_fd_; }

 private:
  std::vector<SP_Channel> getReadyChannels(int);
  static const int MAXFDS = 100000;
  int epoll_fd_;
  SP_Channel fd2chan_[MAXFDS];
  std::vector<epoll_event> epoll_events_;
};

typedef std::shared_ptr<Epoll> SP_Epoll;