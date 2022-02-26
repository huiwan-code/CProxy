#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <utility>
#include "Util.h"
#include "Msg.h"
#include <iostream>

CtlMsg make_ctl_msg(CtlMsgType type, char *data, size_t data_len) {
  CtlMsg msg = CtlMsg{};
  msg.type = type;
  SPDLOG_INFO("send  ctl msg: {}", data);
  SPDLOG_INFO("send  ctl type: {}", msg.type);
  memcpy(msg.data, data, data_len);
  msg.len = sizeof(u_int32_t) + sizeof(CtlMsgType) + data_len;
  return msg;
}

size_t get_ctl_msg_body_size(const CtlMsg& msg) {
  return msg.len - sizeof(CtlMsgType) - sizeof(u_int32_t);
}