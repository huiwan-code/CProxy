#include "control.h"
#include <string.h>
#include "server.h"
#include "lib/util.h"
#include "spdlog/spdlog.h"

void Control::handleNewCtlReq(void *new_ctl_req_msg, SP_CtlConn conn) {
  std::string ctl_id_str = conn->GetCtlId();
  NewCtlRspMsg *req_msg = (NewCtlRspMsg *)malloc(sizeof(NewCtlRspMsg) + ctl_id_str.size() + 1);
  memset(req_msg->ctl_id, 0, ctl_id_str.size() + 1);
  strcpy(req_msg->ctl_id, ctl_id_str.c_str());
  CtlMsg ctl_msg =
      MakeCtlMsg(NewCtlRsp, (char *)req_msg, sizeof(NewCtlRspMsg) + ctl_id_str.size() + 1);
  conn->SendMsg(ctl_msg);
  free(req_msg);
};

// 新隧道处理
void Control::handleNewTunnelReq(void *msg, SP_CtlConn conn) {
  NewTunnelReqMsg *new_tunnel_req_msg = (NewTunnelReqMsg *)msg;
  std::string rand_tun_id = rand_str(5);
  std::cout << rand_tun_id << std::endl;
  // 添加tunnel到control中
  SP_Tunnel tun(new Tunnel(rand_tun_id, server_->publicListenThread_, server_->eventLoopThreadPool_,
                           shared_from_this()));
  upsertTunnel(rand_tun_id, tun);

  // 返回数据
  NewTunnelRspMsg rsp_msg;
  rsp_msg.local_server_port = new_tunnel_req_msg->local_server_port;
  rsp_msg.proxy_server_port = tun->getListenPort();
  strcpy(rsp_msg.tun_id, rand_tun_id.c_str());
  strcpy(rsp_msg.proxy_server_host, (tun->getListenAddr()).c_str());
  strcpy(rsp_msg.local_server_host, new_tunnel_req_msg->local_server_host);
  CtlMsg ctl_msg = MakeCtlMsg(NewTunnelRsp, (char *)&rsp_msg, sizeof(NewTunnelRspMsg));
  conn_->SendMsg(ctl_msg);
};

void Control::notifyClientNeedProxy(std::string tun_id) {
  NotifyClientNeedProxyMsg req_msg;
  strcpy(req_msg.tun_id, tun_id.c_str());
  req_msg.server_proxy_port = server_->getProxyPort();
  CtlMsg ctl_msg =
      MakeCtlMsg(NotifyClientNeedProxy, (char *)&req_msg, sizeof(NotifyClientNeedProxyMsg));
  conn_->SendMsg(ctl_msg);
}

void Control::shutdownFromPublic(std::string tun_id, std::string proxy_id, u_int32_t tran_count) {
  NotifyProxyShutdownPeerConnMsg req_msg;
  req_msg.tran_count = htonl(tran_count);
  strcpy(req_msg.tun_id, tun_id.c_str());
  strcpy(req_msg.proxy_id, proxy_id.c_str());
  CtlMsg ctl_msg = MakeCtlMsg(NotifyProxyShutdownPeerConn, (char *)&req_msg,
                                sizeof(NotifyProxyShutdownPeerConnMsg));
  conn_->SendMsg(ctl_msg);
};

void Control::handleShutdownPublicConn(void *msg, SP_CtlConn conn) {
  NotifyProxyShutdownPeerConnMsg *req_msg = (NotifyProxyShutdownPeerConnMsg *)msg;
  u_int32_t theoreticalTotalRecvCount = ntohl(req_msg->tran_count);
  std::string tun_id = req_msg->tun_id;
  std::string proxy_id = req_msg->proxy_id;
  bool tunIsExist;
  SP_Tunnel tun = tunnel_map_.get(tun_id, tunIsExist);
  if (!tunIsExist) {
    SPDLOG_CRITICAL("tun {} not exist", tun_id);
    return;
  }

  bool proxyConnIsExist;
  SP_ProxyConn proxyConn = tun->getProxyConn(proxy_id, proxyConnIsExist);
  if (!proxyConnIsExist) {
    SPDLOG_CRITICAL("proxy conn {} not exist", proxy_id);
    return;
  }

  proxyConn->incrTheoreticalTotalRecvCount(theoreticalTotalRecvCount);
  tun->shutdownPublicConn(proxyConn);
};

void Control::handleCtlConnClose(SP_CtlConn conn) { server_->control_map_.erase(ctl_id_); };

void Control::handleFreeProxyConnReq(void *msg, SP_CtlConn conn) {
  FreeProxyConnReqMsg *req_msg = (FreeProxyConnReqMsg *)msg;
  std::string tun_id = std::string(req_msg->tun_id);
  std::string proxy_id = std::string(req_msg->proxy_id);
  bool tunIsExist;
  SP_Tunnel tun = tunnel_map_.get(tun_id, tunIsExist);
  if (!tunIsExist) {
    SPDLOG_CRITICAL("tun {} not exist", tun_id);
    return;
  }
  tun->freeProxyConn(proxy_id);
};