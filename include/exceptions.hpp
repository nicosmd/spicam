//
// Copyright (c) 2024 Nico Schmidt
//

#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP

#include <stdexcept>

class DeviceFileError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

#endif //EXCEPTIONS_HPP
