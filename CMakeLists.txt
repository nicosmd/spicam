project(h264streamer)

cmake_minimum_required(VERSION 3.20)

add_executable(h264streamer main.cpp)

add_executable(check_caps check_caps.cpp)

add_executable(dmabuf_test dmabuf_test.cpp dmabuf.cpp)

set_property(TARGET dmabuf_test PROPERTY CXX_STANDARD 17)
