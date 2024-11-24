//
// Copyright (c) 2024 Nico Schmidt
//

#ifndef V4L2_BUFFER_HPP
#define V4L2_BUFFER_HPP

#include <cstdint>
#include <memory>

#include "device_file_handle.hpp"
#include "dmabuf.hpp"

class V4L2VideoBuffer {
private:
    std::weak_ptr<DeviceFileHandle> m_device;
    std::uint32_t m_buffer_type;
    std::uint32_t m_memory_type;
    std::uint32_t m_buffer_size;
    std::uint32_t m_buffer_sizes;
    std::vector<DmaBuf> m_buffers;

    void fill_buffer();
    void request_buffer() const;

public:
    V4L2VideoBuffer(std::weak_ptr<DeviceFileHandle> device, std::uint32_t buffer_type, std::uint32_t memory_type,
                    std::uint32_t buffer_sizes);
};

#endif //V4L2_BUFFER_HPP
