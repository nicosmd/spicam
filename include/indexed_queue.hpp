//
// Copyright (c) 2024 Nico Schmidt
//

#ifndef INDEXED_QUEUE_HPP
#define INDEXED_QUEUE_HPP
#include <cstddef>

template<class T>
class IIndexedQueue {
public:
    virtual ~IIndexedQueue() = default;

    virtual T dequeue() = 0;
    virtual void enqueue(T&& package) = 0;

};

#endif //INDEXED_QUEUE_HPP
