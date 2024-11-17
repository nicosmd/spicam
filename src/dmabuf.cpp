/**
MIT License

Copyright (c) 2022 Matthias Fend <matthias.fend@emfend.at>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
**/

#include "dmabuf.hpp"

#include "dmabuf_operations.hpp"
#include <sys/mman.h>
#include "device_file_handle.hpp"
#include "exceptions.hpp"

DmaBuf::DmaBuf(int heap_fd, size_t size, const std::string &name) {
    m_fd = dmabuf_heap_alloc(heap_fd, name.c_str(), size);
    PLOG_DEBUG << "Allocating dma buffer with fd: " << m_fd;

    if (m_fd < 0) {
        throw DeviceFileError{"Failed to alloc dmabuf"};
    }

    m_map = mmap(0, size, PROT_WRITE | PROT_READ,
                 MAP_SHARED, m_fd, 0);

    if (m_map == MAP_FAILED) {
        throw DeviceFileError{"Failed to map dmabuf"};
    }
    m_size = size;
}

int DmaBuf::get_fd() const {
    return m_fd;
}

void *DmaBuf::get_map() const {
    return m_map;
}

std::size_t DmaBuf::get_size() const {
    return m_size;
}

DmaBuf::~DmaBuf() {
    munmap(m_map, m_size);
}


std::vector<DmaBuf> allocate_dma_bufs(std::uint32_t num_bufs, std::uint32_t bufsize) {
    auto dma_heap_device = DeviceFileHandle{"/dev/dma_heap/linux,cma"};

    std::vector<DmaBuf> dma_bufs;
    dma_bufs.reserve(num_bufs);

    for (int i = 0; i < num_bufs; i++) {
        PLOG_DEBUG << "Allocating dma buffer with size: " << bufsize;

        dma_heap_device.do_file_operation([bufsize, &dma_bufs, i](int fd) {
            dma_bufs.emplace_back(fd, bufsize, "spycambuf_" + std::to_string(i));
        });
    }

    return dma_bufs;
}
