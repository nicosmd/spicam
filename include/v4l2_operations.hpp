//
// Copyright (c) 2024 Nico Schmidt
//

#ifndef V4L2_OPERATIONS_HPP
#define V4L2_OPERATIONS_HPP
#include <linux/videodev2.h>

#include "buffer_info.hpp"
#include "dmabuf.hpp"

v4l2_format set_camera_format(int fd);

v4l2_format set_encoding_format_output(int fd);


v4l2_format set_encoding_format_capture(int fd);

void set_encoding_frame_interval(int fd);


void request_buffers(int fd, std::uint32_t number_buffers, int buffer_tye, int memory_type);

void queue_dma_buffer(int fd, const DmaBuf &dma_buf, std::uint32_t buffer_type, std::uint32_t index);

void queue_dma_buffer(int fd, const std::vector<DmaBuf> &dma_bufs, std::uint32_t buffer_type);

void log_buffer_status(int fd, std::uint32_t buffer_type, std::uint32_t index);

void queue_dma_buffer_mplane(int fd, const DmaBuf &dma_buf, std::uint32_t buffer_tye, std::uint32_t index);

void queue_dma_buffer_mplane(int fd, const DmaBuf &dma_buf, std::uint32_t buffer_tye, const BufferInfo &info,
                             std::uint32_t index);

void queue_dma_buffer_mplane(int fd, const std::vector<DmaBuf> &dma_bufs, std::uint32_t buffer_tye);

BufferInfo dequeue_buffer(int fd, std::uint32_t buffer_type, std::uint32_t memory_type);

std::uint32_t dequeue_buffer_mplane(int fd, std::uint32_t buffer_type, std::uint32_t memory_type);

void log_enum_fmt(int fd, std::uint32_t buffer_type);

void stream_on(int fd, std::uint32_t buffer_type);

void stream_on_capture(int fd);

void stream_on_output(int fd);

void stream_on_output_mplane(int fd);

void stream_on_capture_mplane(int fd);

void stream_off_capture(int fd);

void stream_off_output(int fd);

void stream_off_output_mplane(int fd);

void stream_off_capture_mplane(int fd);

#endif //V4L2_OPERATIONS_HPP
