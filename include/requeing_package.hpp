//
// Copyright (c) 2024 Nico Schmidt
//

#ifndef REQUEINGPACKAGE_HPP
#define REQUEINGPACKAGE_HPP
#include <optional>
#include <utility>

#include "condition.hpp"
#include "indexed_queue.hpp"

template<class T>
class RequeingPackage {
    T m_value;
    std::weak_ptr<IIndexedQueue<RequeingPackage> > m_queue;
    bool m_is_empty = true;

    explicit RequeingPackage(T &&value) : m_value{std::forward<T>(value)}, m_queue{}, m_is_empty(false) {
    }

public:
    RequeingPackage(const RequeingPackage &other) = delete;

    RequeingPackage(RequeingPackage &&other) noexcept
        : m_value(std::move(other.m_value)),
          m_queue(std::move(other.m_queue)) {
        m_is_empty = other.m_is_empty;
        other.m_is_empty = false;
    }

    RequeingPackage &operator=(const RequeingPackage &other) = delete;

    RequeingPackage &operator=(RequeingPackage &&other) noexcept {
        if (this == &other)
            return *this;
        m_value = std::move(other.m_value);
        m_queue = std::move(other.m_queue);
        m_is_empty = other.m_is_empty;
        other.m_is_empty = false;
        return *this;
    }

    bool is_empty() const { return m_is_empty; }

private:
    class PackageBuilder {
        RequeingPackage m_underConstruction;

    public:
        explicit PackageBuilder(T &&value) : m_underConstruction{std::forward<T>(value)} {
        }

        PackageBuilder &with_queue(IIndexedQueue<T> *queue) {
            m_underConstruction.m_queue = queue;
            return *this;
        }

        RequeingPackage &&build() {
            PRECONDITION(m_underConstruction.m_queue.expired(), "Queue is null");
            return std::move(m_underConstruction);
        }

        operator RequeingPackage() {
            return build();
        }
    };

public:
    template<typename... Args>
    static PackageBuilder create(Args &&... args) {
        PackageBuilder builder{T{std::forward<Args>(args)...}};
        return builder;
    }

    ~RequeingPackage() {
        if (auto queue_instance = m_queue.lock()) {
            queue_instance->enqueue(std::move(*this));
        }
    }

    const T &data() const { return m_value; }
};

#endif //REQUEINGPACKAGE_HPP
