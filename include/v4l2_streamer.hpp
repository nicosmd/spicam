//
// Copyright (c) 2024 Nico Schmidt
//

#ifndef V4L2_STREAMER_HPP
#define V4L2_STREAMER_HPP
#include <string>

#include "device_file_handle.hpp"
#include "dmabuf.hpp"


class V4L2Streamer {
    std::size_t m_width;
    std::size_t m_height;
    DeviceFileHandle m_camera;
    DeviceFileHandle m_encoder;
    std::vector<DmaBuf> m_camera_capture_buffers;
    std::vector<DmaBuf> m_encoder_capture_buffers;

public:
    V4L2Streamer(const std::string &camera_device_path, std::size_t width, std::size_t height);

    void start_streaming() const;

    void next_frame();

    ~V4L2Streamer();
};


#endif //V4L2_STREAMER_HPP
