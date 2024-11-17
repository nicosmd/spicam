/*
 * Some basic dmabuf(-heap) helpers.
 * 
 * 2022, Matthias Fend <matthias.fend@emfend.at>
 */

#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include "dmabuf.hpp"

#include <sys/mman.h>

#include "device_file_handle.hpp"
#include "exceptions.hpp"

DmaBuf::DmaBuf(int heap_fd, size_t size, const std::string &name) {
    m_fd = dmabuf_heap_alloc(heap_fd, name.c_str(), size);

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

DmaBuf::~DmaBuf() {
    munmap(m_map, m_size);
}

/*
 * Depending on the configuration method, the name of the device node name
 * of the dmabuf-heap changes. If the CMA area is configured from a device
 * tree node, the heap node is '/dev/dma_heap/linux,cma', otherwise the
 * node is '/dev/dma_heap/reserved'.
 * So let's just try both.
 */
int dmabuf_heap_open() {
    int i;
    static const char *heap_names[] = {"/dev/dma_heap/linux,cma", "/dev/dma_heap/reserved"};

    for (i = 0; i < 2; i++) {
        int fd = open(heap_names[i], O_RDWR, 0);

        if (fd >= 0)
            return fd;
    }

    return -1;
}

void dmabuf_heap_close(int heap_fd) {
    close(heap_fd);
}

int dmabuf_heap_alloc(int heap_fd, const char *name, size_t size) {
    struct dma_heap_allocation_data alloc = {0};

    alloc.len = size;
    alloc.fd_flags = O_CLOEXEC | O_RDWR;

    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc) < 0)
        return -1;

    if (name)
        ioctl(alloc.fd, DMA_BUF_SET_NAME, name);

    return alloc.fd;
}

static int dmabuf_sync(int buf_fd, bool start) {
    struct dma_buf_sync sync = {0};

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
