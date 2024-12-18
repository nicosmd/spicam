//
// Copyright (c) 2024 Nico Schmidt
//

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

    DmaBuf(const DmaBuf &other) = delete;

    DmaBuf(DmaBuf &&other) noexcept
        : m_fd(other.m_fd),
          m_map(other.m_map),
          m_size(other.m_size) {
    }

    DmaBuf & operator=(const DmaBuf &other) = delete;

    DmaBuf & operator=(DmaBuf &&other) noexcept {
        if (this == &other)
            return *this;
        m_fd = other.m_fd;
        m_map = other.m_map;
        m_size = other.m_size;
        return *this;
    }

    [[nodiscard]] int get_fd() const;

    [[nodiscard]] void *get_map() const;

    [[nodiscard]] std::size_t get_size() const;

    ~DmaBuf();
};


[[nodiscard]] std::vector<DmaBuf> allocate_dma_bufs(std::uint32_t num_bufs, std::uint32_t bufsize);

#endif
