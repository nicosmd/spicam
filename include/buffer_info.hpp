//
// Created by nico on 11/17/24.
//

#ifndef BUFFER_INFO_HPP
#define BUFFER_INFO_HPP

#include <cstdint>
#include <bits/types/struct_timeval.h>

struct BufferInfo {
    std::uint32_t index;
    timeval timestamp;
    std::uint32_t bytesused;
    std::uint32_t field;
};

#endif //BUFFER_INFO_HPP