cmake_minimum_required(VERSION 3.20)

project(h264streamer)

find_package(Plog REQUIRED)

cmake_minimum_required(VERSION 3.20)

add_library(v4l2_utils)

target_include_directories(v4l2_utils PUBLIC include)

target_link_libraries(v4l2_utils PUBLIC plog::plog)

set(SOURCE_HEADER
        include/device_file_handle.hpp
        include/dmabuf.hpp
        include/exceptions.hpp
        include/v4l2_streamer.hpp
        include/buffer_info.hpp
        include/v4l2_operations.hpp
)

target_sources(v4l2_utils PRIVATE
        src/dmabuf.cpp
        src/v4l2_streamer.cpp
        src/v4l2_operations.cpp
        src/dmabuf_operations.cpp
        ${SOURCE_HEADER}
)

set_property(TARGET v4l2_utils PROPERTY CXX_STANDARD 23)

add_subdirectory(apps)