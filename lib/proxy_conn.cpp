#include "proxy_conn.h"
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <sys/epoll.h>

#include "util.h"
#include "spdlog/spdlog.h"

void ProxyConn::handleRead() {
  if (is_start_) {
    int bs = splice(fd_, NULL, pipe_fds_[1], NULL, 2048, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
    if (bs < 0) {
      SPDLOG_CRITICAL("proxy_id: {} -> pipe_fd: {} splice err: {}", proxy_id_, pipe_fds_[1],
                      strerror(errno));
      return;
    }
    if (bs == 0) {
      in_buffer_empty_ = true;
      return;
    }
    bs = splice(pipe_fds_[0], NULL, peer_conn_fd_, NULL, bs, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
    if (bs < 0) {
      SPDLOG_CRITICAL("proxy_id {} pipe {} -> peer_conn_fd {} splice err: {}", proxy_id_,
                      pipe_fds_[0], peer_conn_fd_, strerror(errno));
      return;
    }
    incrRecvCount(bs);

    if (getRecvCount() == getTheoreticalTotalRecvCount()) {
      closeLocalPeerConnHandler_(shared_from_this());
    }
    return;
  }
  // 未开始工作，处理ctl信息
  ProxyCtlMsg msg;
  size_t max_msg_len = sizeof(ProxyCtlMsg);
  char *buffer_ptr = (char *)&msg;
  // 读消息体长度
  size_t readNum = readn(fd_, buffer_ptr, sizeof(u_int32_t), in_buffer_empty_);
  if (readNum == 0 && in_buffer_empty_) {
    return;
  }
  if (readNum != sizeof(u_int32_t)) {
    SPDLOG_CRITICAL("proxy_id {} read msg len err", proxy_id_);
    return;
  }
  buffer_ptr += readNum;
  msg.len = ntohl(msg.len);
  // buffer 是否能存放数据
  if (msg.len > max_msg_len) {
    SPDLOG_CRITICAL("msg len too long");
    return;
  }
  // 读取消息体
  size_t body_len = msg.len - sizeof(msg.len);
  readNum = readn(fd_, buffer_ptr, body_len, in_buffer_empty_);
  if (readNum == 0 && in_buffer_empty_) {
    return;
  }
  if (readNum != body_len) {
    SPDLOG_CRITICAL("read msg body err readNum: {}; body_len: {}", readNum, body_len);
    return;
  }
  switch (msg.type) {
    case ProxyMetaSet: {
      ProxyMetaSetMsg proxy_meta_set_msg;
      memcpy(&proxy_meta_set_msg, msg.data, get_proxy_ctl_msg_body_size(msg));
      proxyMetaSetHandler_((void *)&proxy_meta_set_msg, shared_from_this());
      break;
    }
    case StartProxyConnReq: {
      StartProxyConnReqMsg req_msg;
      memcpy(&req_msg, msg.data, get_proxy_ctl_msg_body_size(msg));
      startProxyConnReqHandler_((void *)&req_msg, shared_from_this());
      break;
    }
    case StartProxyConnRsp: {
      StartProxyConnRspMsg rsp_msg;
      memcpy(&rsp_msg, msg.data, get_proxy_ctl_msg_body_size(msg));
      startProxyConnRspHandler_((void *)&rsp_msg, shared_from_this());
      break;
    }
  }
};

void ProxyConn::handleWrite() {
  if (out_buffer_->GetUnreadSize() <= 0) {
    return;
  }
  size_t sent_num = out_buffer_->WriteToSock(fd_);
  if (sent_num <= 0) {
    SPDLOG_CRITICAL("sent to fd err");
  }

  if (out_buffer_->GetUnreadSize() > 0) {
    channel_->AddEvents(EPOLLOUT);
  }
}

void ProxyConn::postHandle() {
  // 对端关闭且本地缓冲区已读完
  if (in_buffer_empty_ && channel_->IsPeerClosed()) {
    close_handler_(shared_from_this());
    return;
  }

  channel_->SetEvents(EPOLLET | EPOLLIN | EPOLLRDHUP);
  if (out_buffer_->GetUnreadSize() > 0) {
    channel_->AddEvents(EPOLLOUT);
  }

  loop_->UpdateToPoller(channel_);
};

void ProxyConn::SendMsg(ProxyCtlMsg &msg) {
  u_int32_t msg_len = msg.len;
  msg.len = htonl(msg.len);

  size_t writeNum = out_buffer_->WriteToBuffer((char *)&msg, msg_len);
  if (writeNum != msg_len) {
    SPDLOG_CRITICAL("send msg err");
    return;
  }
  channel_->AddEvents(EPOLLOUT);
  loop_->UpdateToPoller(channel_);
};

int ProxyConn::send_msg_dirct(ProxyCtlMsg &msg) {
  u_int32_t msg_len = msg.len;
  msg.len = htonl(msg.len);
  size_t writeNum = write(fd_, (char *)&msg, msg_len);
  if (writeNum != msg_len) {
    SPDLOG_CRITICAL("send msg err");
    return -1;
  }
  return 0;
};
// return: proxyConn是否空闲
bool ProxyConn::shutdownFromRemote() {
  std::unique_lock<std::mutex> lock(close_mutex_);
  // 关闭写方向
  // shutdown关闭后，会直接往目标fd的等待队列中添加EPOLLIN，即使当时fd已经没有监听这个事件
  // https://mp.weixin.qq.com/s?__biz=MzUxNDUwOTc0Nw==&mid=2247484253&idx=1&sn=e42ed5e1af8382eb6dffa7550470cdf3
  shutdown(peer_conn_fd_, SHUT_WR);
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
  resetRecvCount();
  resetTranCount();
  resetTheoreticalTotalRecvCount();
  is_start_ = false;
  // peerConn_ = nullptr;
  half_close_ = false;
};

ProxyCtlMsg make_proxy_ctl_msg(ProxyCtlMsgType type, char *data, size_t data_len) {
  ProxyCtlMsg msg = ProxyCtlMsg{};
  msg.type = type;
  memcpy(msg.data, data, data_len);
  msg.len = sizeof(u_int32_t) + sizeof(ProxyCtlMsgType) + data_len;
  return msg;
};

size_t get_proxy_ctl_msg_body_size(const ProxyCtlMsg &msg) {
  return msg.len - sizeof(ProxyCtlMsgType) - sizeof(u_int32_t);
}
