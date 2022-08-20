#include "msg.h"
#include <string.h>

CtlMsg MakeCtlMsg(CtlMsgType type, char* data, size_t data_len) {
  CtlMsg msg = CtlMsg{};
  msg.type = type;
  memcpy(msg.data, data, data_len);
  msg.len = sizeof(u_int32_t) + sizeof(CtlMsgType) + data_len;
  return msg;
}

size_t GetCtlMsgBodySize(const CtlMsg& msg) {
  return msg.len - sizeof(CtlMsgType) - sizeof(u_int32_t);
}