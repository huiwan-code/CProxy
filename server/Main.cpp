#include "Server.h"
int main() {
    int threadNum = 4;
    int ctlPort = 8080;
    int proxyPort = 8089;
    Server server(threadNum, ctlPort, proxyPort);
    server.start();
}