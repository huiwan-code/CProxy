#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include "ProxyConn.h"
#include "Util.h"

void ProxyConn::handleRead() {
  printf("[ProxyConn::handleRead] proxy_id: %s\n", proxy_id_.c_str());
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
    printf("read msg len err\n");
    return;
  }
  buffer_ptr += readNum;
  msg.len = ntohl(msg.len);
  // buffer 是否能存放数据
  if (msg.len > max_msg_len) {
    printf("msg len too long\n");
    return;
  }
  // 读取消息体
  size_t body_len = msg.len - sizeof(msg.len);
  readNum = readn(fd_, buffer_ptr, body_len, inBufferEmpty_);
  if (readNum != body_len) {
    printf("read msg body err\n");
    return;
  }

  printf("[ProxyConn::handleRead] proxy_id: %s; msg.type: %d\n", proxy_id_.c_str(), msg.type);
  switch(msg.type) {
    case ProxyMetaSet:
      {
        ProxyMetaSetMsg proxy_meta_set_msg;
        memcpy(&proxy_meta_set_msg, msg.data, get_proxy_ctl_msg_body_size(msg));
        proxyMetaSetHandler_((void*)&proxy_meta_set_msg, shared_from_this());
        break;
      }
    case StartProxyConnReq:
      {
        StartProxyConnReqMsg req_msg;
        memcpy(&req_msg, msg.data, get_proxy_ctl_msg_body_size(msg));
        startProxyConnReqHandler_((void*)&req_msg, shared_from_this());
      }
    case StartProxyConnRsp:
      {
        StartProxyConnRspMsg rsp_msg;
        memcpy(&rsp_msg, msg.data, get_proxy_ctl_msg_body_size(msg));
        startProxyConnRspHandler_((void*)&rsp_msg, shared_from_this());
      }
  }
};

void ProxyConn::handleWrite() {
  if (out_buffer_->get_unread_size() <= 0) {
    return;
  }
  size_t sent_num = out_buffer_->write_to_sock(fd_);
  printf("[ProxyConn::handleWrite] sent_num: %d\n", sent_num);
  if (sent_num <= 0) {
    printf("sent to fd err");
  }

  if (out_buffer_->get_unread_size() > 0) {
    channel_->addEvents(EPOLLOUT);
  }
}

void ProxyConn::postHandle() {
  // 对端关闭且本地缓冲区已读完
  if (inBufferEmpty_ && channel_->isPeerClosed()) {
    closeHandler_(shared_from_this());
    return;
  }

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
  printf("[ProxyConn::send_msg] writeNum: %d\n", writeNum);
  if (writeNum != msg_len) {
    printf("send msg err");
    return;
  }
  channel_->addEvents(EPOLLOUT);
  loop_->updatePoller(channel_);
};

// return: proxyConn是否空闲
bool ProxyConn::shutdownFromRemote() {
  std::unique_lock<std::mutex> lock(close_mutex_);
  // 关闭写方向
  shutdown(peer_conn_fd_, 1);
  // 如果之前已经是半关闭状态，将localConn与proxyConn断开
  if (half_close_) {
    resetConn();
    return true;
  } else {
    half_close_ = true;
  }
  return false;
};

// return: proxyConn是否空闲
bool ProxyConn::shutdownFromLocal() {
  std::unique_lock<std::mutex> lock(close_mutex_);
  if (half_close_) {
    resetConn();
    return true;
  } else {
    half_close_ = true;
  }
  return false;
};

// 重置连接
void ProxyConn::resetConn() {
  setPeerConnFd(0);
  is_start_ = false;
  peerConn_ = nullptr;
  half_close_ = false;
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



