#pragma once
#include <sys/types.h>

const int MAX_CTL_MSG_LEN = 2048;

enum CtlMsgType {
  NewCtlReq,
  NewCtlRsp,
  NewTunnelReq,
  NewTunnelRsp,
  NotifyClientNeedProxy,
  NotifyProxyShutdownPeerConn,
  FreeProxyConnReq
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
  char local_server_host[20];
  u_int32_t local_server_port;
};

// 定长
struct NewTunnelRspMsg {
  char tun_id[10];
  u_int32_t local_server_port;
  u_int32_t proxy_server_port;
  char proxy_server_host[20];
  char local_server_host[20];
};

// 服务端通知客户端发起创建proxy请求
struct NotifyClientNeedProxyMsg {
  char tun_id[10];
  u_int32_t server_proxy_port;
};

struct NotifyProxyShutdownPeerConnMsg {
  char tun_id[10];
  char proxy_id[10];
  u_int32_t tran_count;
};

struct FreeProxyConnReqMsg {
  char tun_id[10];
  char proxy_id[10];
};

CtlMsg MakeCtlMsg(CtlMsgType type, char* data, size_t data_len);
size_t GetCtlMsgBodySize(const CtlMsg& msg);