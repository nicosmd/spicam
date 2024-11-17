//
// Created by nico on 11/16/24.
//

#ifndef MACRO_UTILS_HPP
#define MACRO_UTILS_HPP

// Helper macros for generating unique variable names
#define CONCATENATE_DETAIL(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_DETAIL(x, y)
#define UNIQUE_NAME(base) CONCATENATE(base, __COUNTER__)

#endif //MACRO_UTILS_HPP
