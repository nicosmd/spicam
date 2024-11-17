//
// Copyright (c) 2024 Nico Schmidt
//

/**
 * This file was derived from https://github.com/emfend/dmabuf-v4l2-demo/blob/main/dmabuf.c using the following license:
 *
 *> MIT License
 *>
 *> Copyright (c) 2022 Matthias Fend <matthias.fend@emfend.at>
 *>
 *> Permission is hereby granted, free of charge, to any person obtaining a copy
 *> of this software and associated documentation files (the "Software"), to deal
 *> in the Software without restriction, including without limitation the rights
 *> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *> copies of the Software, and to permit persons to whom the Software is
 *> furnished to do so, subject to the following conditions:
 *>
 *> The above copyright notice and this permission notice shall be included in all
 *> copies or substantial portions of the Software.
 *>
 *> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *> SOFTWARE.
**/

#include "dmabuf_operations.hpp"

#include <cerrno>
#include <linux/dma-buf.h>
#include <sys/mman.h>
#include <linux/dma-heap.h>
#include <fcntl.h>
#include <sys/ioctl.h>

std::uint32_t dmabuf_heap_alloc(int heap_fd, const char *name, size_t size) {
    dma_heap_allocation_data alloc = {0};

    alloc.len = size;
    alloc.fd_flags = O_CLOEXEC | O_RDWR;

    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc) < 0)
        return -1;

    if (name)
        ioctl(alloc.fd, DMA_BUF_SET_NAME, name);

    return alloc.fd;
}

static int dmabuf_sync(int buf_fd, bool start) {
    dma_buf_sync sync = {0};

    sync.flags = (start ? DMA_BUF_SYNC_START : DMA_BUF_SYNC_END) | DMA_BUF_SYNC_RW;

    do {
        if (ioctl(buf_fd, DMA_BUF_IOCTL_SYNC, &sync) == 0)
            return 0;
    } while ((errno == EINTR) || (errno == EAGAIN));

    return -1;
}

int dmabuf_sync_start(int buf_fd) {
    return dmabuf_sync(buf_fd, true);
}

int dmabuf_sync_stop(int buf_fd) {
    return dmabuf_sync(buf_fd, false);
}
