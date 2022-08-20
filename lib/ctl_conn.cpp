#include <netdb.h>
#include <string.h>
#include <exception>
#include <functional>

#include "buffer.h"
#include "ctl_conn.h"
#include "msg.h"
#include "util.h"
#include "spdlog/spdlog.h"

void CtlConn::handleRead() try {
  // 从fd中读取ctl_msg数据，
  CtlMsg msg;
  size_t max_msg_len = sizeof(CtlMsg);
  char *buffer_ptr = (char *)&msg;
  // 读消息体长度
  size_t read_num = readn(fd_, buffer_ptr, sizeof(u_int32_t), in_buffer_empty_);
  if (read_num == 0 && in_buffer_empty_) {
    return;
  }
  if (read_num != sizeof(u_int32_t)) {
    SPDLOG_CRITICAL("read msg len err");
    return;
  }
  buffer_ptr += read_num;
  msg.len = ntohl(msg.len);
  // buffer 是否能存放数据
  if (msg.len > max_msg_len) {
    SPDLOG_CRITICAL("msg len too long: %d", msg.len);
    return;
  }
  // 读取消息体
  size_t body_len = msg.len - sizeof(msg.len);
  read_num = readn(fd_, buffer_ptr, body_len, in_buffer_empty_);
  if (read_num == 0 && in_buffer_empty_) {
    return;
  }
  if (read_num != body_len) {
    SPDLOG_CRITICAL("read msg body err read_num: %d; body_len: %d", read_num, body_len);
    return;
  }

  switch (msg.type) {
    case NewCtlReq: {
      NewCtlReqMsg new_ctl_req_msg;
      memcpy(&new_ctl_req_msg, msg.data, GetCtlMsgBodySize(msg));
      new_ctl_req_handler_((void *)&new_ctl_req_msg, shared_from_this());
      break;
    }
    case NewCtlRsp: {
      size_t msg_data_size = GetCtlMsgBodySize(msg);
      NewCtlRspMsg *new_ctl_rsp_msg = (NewCtlRspMsg *)malloc(sizeof(NewCtlRspMsg) + msg_data_size);
      memset(new_ctl_rsp_msg->ctl_id, 0, msg_data_size);
      memcpy(new_ctl_rsp_msg, msg.data, msg_data_size);
      new_ctl_rsp_handler_((void *)new_ctl_rsp_msg, shared_from_this());
      free(new_ctl_rsp_msg);
      break;
    }
    case NewTunnelReq: {
      NewTunnelReqMsg new_tunnel_req_msg;
      memcpy(&new_tunnel_req_msg, msg.data, GetCtlMsgBodySize(msg));
      new_tunnel_req_handler_((void *)&new_tunnel_req_msg, shared_from_this());
      break;
    }
    case NewTunnelRsp: {
      NewTunnelRspMsg new_tunnel_rsp_msg;
      memcpy(&new_tunnel_rsp_msg, msg.data, GetCtlMsgBodySize(msg));
      new_tunnel_rsp_handler_((void *)&new_tunnel_rsp_msg, shared_from_this());
      break;
    }
    case NotifyClientNeedProxy: {
      NotifyClientNeedProxyMsg req_msg;
      memcpy(&req_msg, msg.data, GetCtlMsgBodySize(msg));
      notify_client_need_proxy_handler_((void *)&req_msg, shared_from_this());
      break;
    }
    case NotifyProxyShutdownPeerConn: {
      NotifyProxyShutdownPeerConnMsg req_msg;
      memcpy(&req_msg, msg.data, GetCtlMsgBodySize(msg));
      notify_proxy_shutdown_peer_conn_handler_((void *)&req_msg, shared_from_this());
      break;
    }
    case FreeProxyConnReq: {
      FreeProxyConnReqMsg req_msg;
      memcpy(&req_msg, msg.data, GetCtlMsgBodySize(msg));
      free_proxy_conn_req_handler_((void *)&req_msg, shared_from_this());
      break;
    }
  }
} catch (const std::exception &e) {
  std::cout << "read ctl_conn except: " << e.what() << std::endl;
  abort();
}

void CtlConn::handleWrite() {
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

void CtlConn::postHandle() {
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

// 线程安全
void CtlConn::SendMsg(CtlMsg &msg) {
  std::unique_lock<std::mutex> lock(mutex_);
  u_int32_t msg_len = msg.len;
  msg.len = htonl(msg.len);

  size_t write_num = out_buffer_->WriteToBuffer((char *)&msg, msg_len);
  if (write_num != msg_len) {
    SPDLOG_CRITICAL("send msg err");
    return;
  }
  channel_->AddEvents(EPOLLOUT);
  loop_->UpdateToPoller(channel_);
}
