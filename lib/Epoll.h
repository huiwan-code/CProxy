#pragma once
#include <sys/epoll.h>
#include <memory>
#include <vector>
#include "channel.h"
#include "event_dispatcher.h"

class Epoll : public EventDispatcher {
 public:
  Epoll();
  virtual ~Epoll(){};
  virtual void PollAdd(SP_Channel) override final;
  virtual void PollMod(SP_Channel) override final;
  virtual void PollDel(SP_Channel) override final;
  virtual std::vector<SP_Channel> WaitForReadyChannels() override final;
  int get_fd() { return epoll_fd_; }

 private:
  std::vector<SP_Channel> getReadyChannels(int);
  static const int MAXFDS = 100000;
  int epoll_fd_;
  SP_Channel fd2chan_[MAXFDS];
  std::vector<epoll_event> epoll_events_;
};

typedef std::shared_ptr<Epoll> SP_Epoll;