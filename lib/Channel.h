#pragma once
#include <memory>
#include <unistd.h>

class ChannelOwner {};
typedef std::shared_ptr<ChannelOwner> SP_ChannelOwner;

class Channel {
  public:
    typedef std::function<void()> EventHandler;
    Channel(int fd): fd_(fd) {};
    ~Channel(){
      close(fd_);}
    void setFd(int fd) {fd_=fd;};
    int getFd() {return fd_;};
    void setEvents(__uint32_t events) {events_ = events;};
    void setRevents(__uint32_t events) {revents_ = events;};
    void addEvents(__uint32_t events) {events_ |= events;}
    __uint32_t getEvents() {return events_;}
    __uint32_t getRevents() {return revents_;}
    void setReadHandler(EventHandler handler) {readHandler_ = handler;};
    void setWriteHandler(EventHandler handler) {writeHandler_ = handler;};
    void setErrorHandler(EventHandler handler) {errorHandler_ = handler;}
    void setPostHandler(EventHandler handler) {postHandler_ = handler;}
    void setChannelOwner(SP_ChannelOwner owner) {belong_to_ = owner;}
    void handleEvents();
    bool isPeerClosed() {return peerClosed_;}
  private:
    int fd_;
    __uint32_t events_;
    __uint32_t revents_;
    EventHandler readHandler_ = [](){};
    EventHandler errorHandler_ = [](){};
    EventHandler writeHandler_ = [](){};
    // 每次事件处理后，对当前channel的事件更新或将当前channel从epoll中去掉
    EventHandler postHandler_ = [](){};
    SP_ChannelOwner belong_to_;
    bool peerClosed_;
};

typedef std::shared_ptr<Channel> SP_Channel;