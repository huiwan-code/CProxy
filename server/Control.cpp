#include <string.h>
#include "Control.h"
#include "lib/Util.h"
#include "Server.h"

void Control::handleNewCtlReq(void* new_ctl_req_msg, SP_CtlConn conn) {
  std::string ctl_id_str = conn->get_ctl_id();
  NewCtlRspMsg *req_msg = (NewCtlRspMsg *)malloc(sizeof(NewCtlRspMsg) + ctl_id_str.size() + 1);
  memset(req_msg->ctl_id, 0, ctl_id_str.size() + 1);
  strcpy(req_msg->ctl_id, ctl_id_str.c_str());
  CtlMsg ctl_msg = make_ctl_msg(NewCtlRsp, (char *)req_msg, sizeof(NewCtlRspMsg) + ctl_id_str.size() + 1);
  conn->send_msg(ctl_msg);
  free(req_msg);
};

// 新隧道处理
void Control::handleNewTunnelReq(void *msg, SP_CtlConn conn) {
  NewTunnelReqMsg *new_tunnel_req_msg = (NewTunnelReqMsg *)msg;
  std::string rand_tun_id = rand_str(5);
  std::cout << rand_tun_id << std::endl;
  // 添加tunnel到control中
  SP_Tunnel tun(new Tunnel(rand_tun_id, new_tunnel_req_msg->server_port, server_->publicListenThread_, server_->eventLoopThreadPool_, shared_from_this()));
  upsertTunnel(rand_tun_id, tun);

  // 返回数据
  NewTunnelRspMsg rsp_msg;
  rsp_msg.server_port = new_tunnel_req_msg->server_port;
  strcpy(rsp_msg.tun_id, rand_tun_id.c_str());
  CtlMsg ctl_msg = make_ctl_msg(NewTunnelRsp, (char *)&rsp_msg, sizeof(NewTunnelRspMsg));
  conn_->send_msg(ctl_msg);
};

void Control::notifyClientNeedProxy(std::string tun_id) {
  NotifyClientNeedProxyMsg req_msg;
  strcpy(req_msg.tun_id, tun_id.c_str());
  req_msg.server_proxy_port = server_->getProxyPort();
  CtlMsg ctl_msg = make_ctl_msg(NotifyClientNeedProxy, (char *)&req_msg, sizeof(NotifyClientNeedProxyMsg));
  conn_->send_msg(ctl_msg);
}

void Control::handleCtlConnClose(SP_CtlConn conn) {
  server_->control_map_.erase(ctl_id_);
};