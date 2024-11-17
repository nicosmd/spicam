//
// Created by nico on 11/16/24.
//

#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP

#include <stdexcept>

class DeviceFileError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

#endif //EXCEPTIONS_HPP
