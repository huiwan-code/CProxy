#pragma once
#include <unistd.h>
#include <functional>
#include <memory>
class ChannelOwner {};
typedef std::shared_ptr<ChannelOwner> SP_ChannelOwner;

class Channel {
 public:
  typedef std::function<void()> EventHandler;
  Channel(int fd) : fd_(fd), need_close_when_delete_(true){};
  ~Channel() {
    if (need_close_when_delete_) {
      close(fd_);
    }
  }
  void setFd(int fd) { fd_ = fd; };
  int getFd() { return fd_; };
  void SetEvents(__uint32_t events) { events_ = events; };
  void SetRevents(__uint32_t events) { revents_ = events; };
  void AddEvents(__uint32_t events) { events_ |= events; }
  __uint32_t GetEvents() { return events_; }
  __uint32_t GetRevents() { return revents_; }
  void SetReadHandler(EventHandler handler) { read_handler_ = handler; };
  void SetWriteHandler(EventHandler handler) { write_handler_ = handler; };
  void SetErrorHandler(EventHandler handler) { error_handler_ = handler; }
  void SetPostHandler(EventHandler handler) { post_handler_ = handler; }
  void SetChannelOwner(SP_ChannelOwner owner) { belong_to_ = owner; }
  void HandleEvents();
  bool IsPeerClosed() { return peer_closed_; }
  void SetNeedCloseWhenDelete(bool needClose) { need_close_when_delete_ = needClose; }

 private:
  int fd_;
  bool need_close_when_delete_;
  __uint32_t events_;
  __uint32_t revents_;
  EventHandler read_handler_ = []() {};
  EventHandler error_handler_ = []() {};
  EventHandler write_handler_ = []() {};
  // 每次事件处理后，对当前channel的事件更新或将当前channel从epoll中去掉
  EventHandler post_handler_ = []() {};
  SP_ChannelOwner belong_to_;
  bool peer_closed_;
};

typedef std::shared_ptr<Channel> SP_Channel;