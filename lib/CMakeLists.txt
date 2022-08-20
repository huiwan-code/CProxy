set(lib
    buffer.cpp
    event_loop_thread.cpp
    event_loop_thread_pool.cpp
    util.cpp
    event_loop.cpp
    channel.cpp
    epoll.cpp
    msg.cpp
    ctl_conn.cpp
    proxy_conn.cpp
)

add_library(lib ${lib})
# 使用多线程需要引入的静态库
target_link_libraries(lib pthread)