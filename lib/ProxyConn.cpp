#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include "ProxyConn.h"
#include "Util.h"

void ProxyConn::handleRead() {
  if (is_start_) {
    int bs = splice(fd_, NULL, pipe_fds_[1], NULL, 4096, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
    bs = splice(pipe_fds_[0], NULL, peer_conn_fd_, NULL, bs, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
    printf("[proxyConn]send_bs = %d\n", bs);
    return;
  }
  // 未开始工作，处理ctl信息
  ProxyCtlMsg msg;
  size_t max_msg_len = sizeof(ProxyCtlMsg);
  char *buffer_ptr = (char *)&msg;
  // 读消息体长度
  size_t readNum = readn(fd_, buffer_ptr, sizeof(u_int32_t), inBufferEmpty_);
  if (readNum != sizeof(u_int32_t)) {
    printf("read msg len err");
    return;
  }
  buffer_ptr += readNum;
  msg.len = ntohl(msg.len);
  // buffer 是否能存放数据
  if (msg.len > max_msg_len) {
    printf("msg len too long");
    return;
  }
  // 读取消息体
  size_t body_len = msg.len - sizeof(msg.len);
  readNum = readn(fd_, buffer_ptr, body_len, inBufferEmpty_);
  if (readNum != body_len) {
    printf("read msg body err");
    return;
  }

  switch(msg.type) {
    case ProxyMetaSet:
      {
        ProxyMetaSetMsg proxy_meta_set_msg;
        memcpy(&proxy_meta_set_msg, msg.data, get_proxy_ctl_msg_body_size(msg));
        proxyMetaSetHandler_((void*)&proxy_meta_set_msg, shared_from_this());
        break;
      }
  }
};

void ProxyConn::handleWrite() {
  if (out_buffer_->get_unread_size() <= 0) {
    return;
  }
  size_t sent_num = out_buffer_->write_to_sock(fd_);
  if (sent_num <= 0) {
    printf("sent to fd err");
  }

  if (out_buffer_->get_unread_size() > 0) {
    channel_->addEvents(EPOLLOUT);
  }
}

void ProxyConn::postHandle() {
  // // 对端关闭且本地缓冲区已读完
  // if (inBufferEmpty_ && channel_->isPeerClosed()) {
  //   closeHandler_(shared_from_this());
  //   return;
  // }

  channel_->setEvents(EPOLLET | EPOLLIN  | EPOLLRDHUP);
  if (out_buffer_->get_unread_size() > 0) {
    channel_->addEvents(EPOLLOUT);
  }
  
  loop_->updatePoller(channel_);
};

void ProxyConn::send_msg(ProxyCtlMsg& msg) {
  u_int32_t msg_len = msg.len;
  msg.len = htonl(msg.len);

  size_t writeNum = out_buffer_->write_to_buffer((char *)&msg, msg_len);
  if (writeNum != msg_len) {
    printf("send msg err");
    return;
  }
  channel_->addEvents(EPOLLOUT);
  loop_->updatePoller(channel_);
};

ProxyCtlMsg make_proxy_ctl_msg(ProxyCtlMsgType type, char *data, size_t data_len) {
  ProxyCtlMsg msg = ProxyCtlMsg{};
  msg.type = type;
  memcpy(msg.data, data, data_len);
  msg.len = sizeof(u_int32_t) + sizeof(ProxyCtlMsgType) + data_len;
  return msg;
};

size_t get_proxy_ctl_msg_body_size(const ProxyCtlMsg& msg) {
  return msg.len - sizeof(ProxyCtlMsgType) - sizeof(u_int32_t);
}