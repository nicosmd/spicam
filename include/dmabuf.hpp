
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


[[nodiscard]] std::vector<DmaBuf> allocate_dma_bufs(std::uint32_t num_bufs, std::uint32_t bufsize);

#endif
