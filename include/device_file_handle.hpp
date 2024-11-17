//
// Copyright (c) 2024 Nico Schmidt
//

#ifndef DEVICE_FILE_HANDLE_HPP
#define DEVICE_FILE_HANDLE_HPP

#include <fcntl.h>
#include <unistd.h>

#include "exceptions.hpp"

class DeviceFileHandle {
    int fd;

public:
    explicit DeviceFileHandle(const std::string &path, int flags = O_RDWR) {
        int cam_fd = open(path.c_str(), flags, 0);

        if (cam_fd == -1) {
            throw DeviceFileError{"Failed to open device file at path " + path};
        }

        fd = cam_fd;
    }

    DeviceFileHandle(const DeviceFileHandle &other) = delete;

    DeviceFileHandle(DeviceFileHandle &&other) noexcept
        : fd(other.fd) {
    }

    DeviceFileHandle &operator=(const DeviceFileHandle &other) = delete;

    DeviceFileHandle &operator=(DeviceFileHandle &&other) noexcept {
        if (this == &other)
            return *this;
        fd = other.fd;
        return *this;
    }

    template <typename Callback>
    auto do_file_operation(Callback&& cb) const -> decltype(cb(fd)) {
        return std::forward<Callback>(cb)(fd);
    }

    virtual ~DeviceFileHandle() {
        close(fd);
    }
};

#endif //DEVICE_FILE_HANDLE_HPP
