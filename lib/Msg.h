#pragma once
#include <sys/types.h>
#include <string>

const int MAX_CTL_MSG_LEN = 2048;


enum CtlMsgType {
  NewCtlReq,
  NewCtlRsp,
  NewTunnelReq,
  NewTunnelRsp,
  NotifyClientNeedProxy
};

struct CtlMsg {
  u_int32_t len;
  CtlMsgType type;
  char data[MAX_CTL_MSG_LEN];
};

struct NewCtlReqMsg {};

// Socket发送和接收变长结构体 https://blog.csdn.net/wujinqiao88/article/details/38428557
struct NewCtlRspMsg {
  char ctl_id[0];
};

struct NewTunnelReqMsg {
  u_int32_t server_port;
};

// 定长
struct NewTunnelRspMsg {
  char tun_id[10];
  u_int32_t server_port;
};

// 服务端通知客户端发起创建proxy请求
struct NotifyClientNeedProxyMsg {
  char tun_id[10];
  u_int32_t server_proxy_port;
};

CtlMsg make_ctl_msg(CtlMsgType type, char *data, size_t data_len);
size_t get_ctl_msg_body_size(const CtlMsg& msg);