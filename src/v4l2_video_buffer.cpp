//
// Copyright (c) 2024 Nico Schmidt
//

#include "v4l2_video_buffer.hpp"

#include <utility>
#include <linux/videodev2.h>
#include "condition.hpp"
#include "v4l2_operations.hpp"

bool is_mplane(std::uint32_t buffer_type) {
    return buffer_type != V4L2_BUF_TYPE_VIDEO_CAPTURE && buffer_type != V4L2_BUF_TYPE_VIDEO_OUTPUT;
}

bool is_output_buffer(std::uint32_t buffer_type) {
    return buffer_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE || buffer_type == V4L2_BUF_TYPE_VIDEO_OUTPUT;
}

std::uint32_t get_default_buffer_size(std::uint32_t buffer_type) {
    if (is_output_buffer(buffer_type)) {
        return 1;
    }
    return 8;
}

void V4L2VideoBuffer::fill_buffer() {
    PRECONDITION(!m_device.expired(), "Device handle is already expired");

    m_buffers = allocate_dma_bufs(m_buffer_size, m_buffer_sizes);

    const auto device_instance = m_device.lock();

    if (is_mplane(m_buffer_type)) {
        device_instance->do_file_operation([this](int fd) {
            queue_dma_buffer_mplane(fd, m_buffers, m_memory_type);
        });
    } else {
        device_instance->do_file_operation([this](int fd) {
            queue_dma_buffer(fd, m_buffers, m_memory_type);
        });
    }
}

void V4L2VideoBuffer::request_buffer() const {
    PRECONDITION(!m_device.expired(), "Device handle is already expired");

    const auto device_instance = m_device.lock();

    device_instance->do_file_operation([this](int fd) {
        request_buffers(fd, m_buffer_size, m_buffer_type, m_memory_type);
    });
}

V4L2VideoBuffer::V4L2VideoBuffer(std::weak_ptr<DeviceFileHandle> device,
                                 const std::uint32_t buffer_type,
                                 const std::uint32_t memory_type,
                                 const std::uint32_t buffer_sizes
) : m_device(std::move(device)),
    m_buffer_type(buffer_type),
    m_memory_type(memory_type),
    m_buffer_size(get_default_buffer_size(buffer_type)),
    m_buffer_sizes(buffer_sizes) {
    PRECONDITION(memory_type == V4L2_MEMORY_DMABUF, "Currently only DMABUF memory type supported");

    request_buffer();

    if (!is_output_buffer(m_buffer_type)) {
        fill_buffer();
    }
}
