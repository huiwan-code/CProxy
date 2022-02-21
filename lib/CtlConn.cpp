#include <functional>
#include <netdb.h>
#include <exception>
#include <string.h>

#include "CtlConn.h"
#include "Msg.h"
#include "Buffer.h"
#include "Util.h"

void CtlConn::handleRead() 
try{
  // 从fd中读取ctl_msg数据，
  CtlMsg msg;
  size_t max_msg_len = sizeof(CtlMsg);
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
    case NewCtlReq:
      {
        NewCtlReqMsg new_ctl_req_msg;
        memcpy(&new_ctl_req_msg, msg.data, get_ctl_msg_body_size(msg));
        newCtlReqHandler_((void*)&new_ctl_req_msg, shared_from_this());
        break;
      }
    case NewCtlRsp:
      {
        size_t msg_data_size = get_ctl_msg_body_size(msg);
        NewCtlRspMsg *new_ctl_rsp_msg = (NewCtlRspMsg *)malloc(sizeof(NewCtlRspMsg) + msg_data_size);
        memset(new_ctl_rsp_msg->ctl_id, 0, msg_data_size);
        memcpy(new_ctl_rsp_msg, msg.data, msg_data_size);
        newCtlRspHandler_((void*)new_ctl_rsp_msg, shared_from_this());
        free(new_ctl_rsp_msg);
        break;
      }
    case NewTunnelReq:
      {
        NewTunnelReqMsg new_tunnel_req_msg;
        memcpy(&new_tunnel_req_msg, msg.data, get_ctl_msg_body_size(msg));
        newTunnelReqHandler_((void*)&new_tunnel_req_msg, shared_from_this());
        break;
      }
    case NewTunnelRsp:
      {
        NewTunnelRspMsg new_tunnel_rsp_msg;
        memcpy(&new_tunnel_rsp_msg, msg.data, get_ctl_msg_body_size(msg));
        newTunnelRspHandler_((void*)&new_tunnel_rsp_msg, shared_from_this());
        break;
      }
    case NotifyClientNeedProxy:
      {
        NotifyClientNeedProxyMsg req_msg;
        memcpy(&req_msg, msg.data, get_ctl_msg_body_size(msg));
        notifyClientNeedProxyHandler_((void*)&req_msg, shared_from_this());
        break;
      }
    case NotifyProxyShutdownPeerConn:
      {
        NotifyProxyShutdownPeerConnMsg req_msg;
        memcpy(&req_msg, msg.data, get_ctl_msg_body_size(msg));
        notifyProxyShutdownPeerConnHandler_((void*)&req_msg, shared_from_this());
      }
    case FreeProxyConnReq:
      {
        FreeProxyConnReqMsg req_msg;
        memcpy(&req_msg, msg.data, get_ctl_msg_body_size(msg));
        freeProxyConnReqHandler_((void*)&req_msg, shared_from_this());
      }
  }
}
catch (const std::exception& e) {
  std::cout << e.what() << std::endl;
  abort();
}

void CtlConn::handleWrite() {
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

void CtlConn::postHandle() {
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

// 线程安全
void CtlConn::send_msg(CtlMsg& msg) {
  std::unique_lock<std::mutex> lock(mutex_);
  u_int32_t msg_len = msg.len;
  msg.len = htonl(msg.len);

  size_t writeNum = out_buffer_->write_to_buffer((char *)&msg, msg_len);
  if (writeNum != msg_len) {
    printf("send msg err");
    return;
  }
  channel_->addEvents(EPOLLOUT);
  loop_->updatePoller(channel_);
}

