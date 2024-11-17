//
// Created by nico on 11/16/24.
//

#ifndef YIELD_HPP
#define YIELD_HPP

#include <functional>
#include "macro_utils.h"

class Yield {
private:
    std::function<void()> m_yield_cb;

public:
    Yield(std::function<void()> cb) : m_yield_cb(std::move(cb)) {
    }

    ~Yield() noexcept { m_yield_cb(); }
};



#endif //YIELD_HPP
