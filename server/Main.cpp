#include "Server.h"
#include "spdlog/spdlog.h"

int main() {
    // 初始化日志格式
    spdlog::set_pattern("[%@ %H:%M:%S:%e %z] [%^%L%$] [thread %t] %v");
    int threadNum = 1;
    int ctlPort = 8080;
    int proxyPort = 8089;
    Server server(threadNum, ctlPort, proxyPort);
    server.start();
}