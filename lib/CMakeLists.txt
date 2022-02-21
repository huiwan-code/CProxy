set(lib
    Buffer.cpp
    EventLoopThread.cpp
    EventLoopThreadPool.cpp
    Util.cpp
    EventLoop.cpp
    Channel.cpp
    Epoll.cpp
    Msg.cpp
    CtlConn.cpp
    ProxyConn.cpp
)

add_library(lib ${lib})
# 使用多线程需要引入的静态库
target_link_libraries(lib pthread)