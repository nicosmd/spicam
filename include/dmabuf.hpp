/*
 * Some basic dmabuf(-heap) helpers.
 * 
 * 2022, Matthias Fend <matthias.fend@emfend.at>
 */
#ifndef DMABUF_H_
#define DMABUF_H_

#include <cstdint>
#include <plog/Log.h>

class DmaBuf {
    int m_fd{0};
    void *m_map{nullptr};
    std::size_t m_size{0};

public:
    DmaBuf(int heap_fd, size_t size, const std::string &name = {});

    [[nodiscard]] int get_fd() const;

    [[nodiscard]] void *get_map() const;

    [[nodiscard]] std::size_t get_size() const;

    ~DmaBuf();
};

int dmabuf_heap_open();

void dmabuf_heap_close(int heap_fd);

int dmabuf_heap_alloc(int heap_fd, const char *name, std::size_t size);

int dmabuf_sync_start(int buf_fd);

int dmabuf_sync_stop(int buf_fd);

[[nodiscard]] std::vector<DmaBuf> allocate_dma_bufs(std::uint32_t num_bufs, std::uint32_t bufsize);

#endif
