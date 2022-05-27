#pragma once
#include <sys/types.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

int socketBindListen(int port);
// 忽略掉sigpipe，避免由此异常导致程序退出
void ignoreSigpipe();

int setfdNonBlock(int);

size_t readn(int fd, char* buffer, size_t size, bool& bufferEmpty);

size_t writen(int fd, const char* buffer, size_t size);

int tcp_connect(const char* host, u_int32_t server_port);

std::string rand_str(int len);

void parse_host_port(char* addr, std::string& host, u_int32_t& port);
typedef std::function<void()> voidFunctor;

template <typename keyT, typename valueT>
struct safe_unordered_map {
  std::mutex mutex_;
  std::unordered_map<keyT, valueT> map;
  bool isExist(keyT key) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (map.find(key) == map.end()) {
      return false;
    } else {
      return true;
    }
  }

  void erase(keyT key) {
    std::unique_lock<std::mutex> lock(mutex_);
    map.erase(key);
  }

  valueT get(keyT key, bool& isExist) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (map.find(key) == map.end()) {
      isExist = false;
      return valueT{};
    } else {
      isExist = true;
    }
    return map[key];
  }

  valueT get(keyT key) {
    std::unique_lock<std::mutex> lock(mutex_);
    return map[key];
  }

  void add(keyT key, valueT val) {
    std::unique_lock<std::mutex> lock(mutex_);
    map.emplace(key, val);
  }

  bool empty() { return map.empty(); }
};
