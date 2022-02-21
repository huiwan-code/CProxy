#include "Channel.h"

#include <sys/epoll.h>

void Channel::handleEvents() {
  // 将events设置为0，默认不
  events_ = 0;
  if (!revents_) return;
  // 对端close，回复rst，设置channel的quiting为true
  if (revents_ & EPOLLHUP) {
    // printf("%d EPOLLHUP\n", fd_);
    peerClosed_ = true;
  }
  // 本端操作一些动作触发了报错，比如对端关闭后，还继续往对端写，write会成功返回，但在wait时会收到这个事件
  if (revents_ & EPOLLERR) {
    // printf("%d EPOLLERR\n", fd_);
    errorHandler_();
  }
  // 对端调用close时，本端会收到RDHUP，EPOLLRDHUP想要被触发，需要显式地在epoll_ctl调用时设置在events中，（此时本端可能还有数据可接收？）                   
  if (revents_ & EPOLLRDHUP) {
    // printf("%d EPOLLRDHUP\n", fd_);
    peerClosed_ = true;
  }
  
  // 数据可读
  if (revents_ & (EPOLLIN | EPOLLPRI)) {
    // printf("%d EPOLLIN\n", fd_);
    readHandler_();
  }
  // 当发送缓冲区可写且对端没关闭
  if ((revents_ & EPOLLOUT) &&  !peerClosed_) {
    // printf("%d EPOLLOUT\n", fd_);
    writeHandler_();
  }
  postHandler_();
}