cmake_minimum_required(VERSION 3.14)
project(reactor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

# 编译选项
add_compile_options(-Wall -Wextra -O2)

# ── 核心库 ──────────────────────────────────────────────────────────
set(REACTOR_SRCS
    src/Buffer.cpp
    src/Channel.cpp
    src/EventLoop.cpp
    src/ThreadPool.cpp
    src/TimerWheel.cpp
    src/Acceptor.cpp
    src/TcpConnection.cpp
    src/TcpServer.cpp
)

add_library(reactor_core STATIC ${REACTOR_SRCS})
target_include_directories(reactor_core PUBLIC src)
target_link_libraries(reactor_core pthread)

# ── Echo Server 示例 ────────────────────────────────────────────────
add_executable(echo_server example/echo_server.cpp)
target_link_libraries(echo_server reactor_core)
