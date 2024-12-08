//
// Copyright (c) 2024 Nico Schmidt
//

#include "v4l2_video_buffer.hpp"

#include <complex>
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
    auto dmabufs = allocate_dma_bufs(m_buffer_size, m_buffer_sizes);

    for (auto &buffer: dmabufs) {
        m_buffers.push_back(std::move(RequeingPackage<DmaBuf>::create(std::move(buffer))));
    }

    const auto device_instance = m_device.lock();

    if (is_mplane(m_buffer_type)) {
        device_instance->do_file_operation([this](int fd) {
            for (int i = 0; i < m_buffers.size(); i++) {
                queue_dma_buffer_mplane(fd, m_buffers[i].data(), m_memory_type, i);
            }
        });
    } else {
        device_instance->do_file_operation([this](int fd) {
            for (int i = 0; i < m_buffers.size(); i++) {
                queue_dma_buffer(fd, m_buffers[i].data(), m_memory_type, i);
            }
        });
    }
}

RequeingPackage<DmaBuf> V4L2VideoBuffer::dequeue() {
    PRECONDITION(!m_device.expired(), "Device handle is already expired");

    const auto device_instance = m_device.lock();

    auto image_buffer_info = device_instance->do_file_operation([this](int fd) {
        return dequeue_buffer(fd, m_buffer_type, m_memory_type);
    });

    auto package = std::move(m_buffers.at(image_buffer_info.index));

    return package;
}

void V4L2VideoBuffer::enqueue(RequeingPackage<DmaBuf> package) {
    for (auto &buffer: m_buffers) {
        if (buffer.is_empty()) {
            buffer = std::move(package);
        }
    }

    throw std::runtime_error("Buffer is full");
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
