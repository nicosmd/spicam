//
// Copyright (c) 2024 Nico Schmidt
//

#ifndef LOG_ASSERT_HPP
#define LOG_ASSERT_HPP

#include <plog/Log.h>

#ifndef NDEBUG
#define PRECONDITION(condition, hint) if (!(condition)) { PLOGE << "Precondition " << #condition << " failed: " << #hint ;  assert(false); }
#else
#define PRECONDITION(condition, hint)
#endif
#endif //LOG_ASSERT_HPP
