//
// Copyright (c) 2024 Nico Schmidt
//

#include <gtest/gtest.h>

#include <utility>

#include "indexed_queue.hpp"
#include "requeing_package.hpp"

class PayloadMock {
private:
  std::string data;

public:
  explicit PayloadMock(std::string data) : data(std::move(data)) {}

  PayloadMock(const PayloadMock &other) = delete;

  PayloadMock(PayloadMock &&other) noexcept : data(std::move(other.data)) {}

  PayloadMock &operator=(const PayloadMock &other) = delete;

  PayloadMock &operator=(PayloadMock &&other) noexcept {
    if (this == &other)
      return *this;
    data = std::move(other.data);
    return *this;
  }

  [[nodiscard]] std::string get_data() const { return data; }
};

class RequeueMock : public IIndexedQueue<RequeingPackage<PayloadMock>>,
                    public std::enable_shared_from_this<RequeueMock> {
  std::vector<RequeingPackage<PayloadMock>> queue;

public:
  RequeueMock() {}

  void fill() {
    for (int i = 0; i < 5; i++) {
      queue.push_back(
          RequeingPackage<PayloadMock>::create("package_" + std::to_string(i))
              .with_queue(weak_from_this()));
    }
  }

  RequeingPackage<PayloadMock> dequeue() override {
    return std::move(queue.back());
  }

  void enqueue(RequeingPackage<PayloadMock> &&package) override {
    for (auto &p : queue) {
      if (p.is_empty() == true) {
        p = std::move(package);
        return;
      }
    }
  }

  std::size_t count_empty() {
    std::size_t count = 0;
    for (const auto &p : queue) {
      if (p.is_empty() == false) {
        count++;
      }
    }
    return count;
  }

  ~RequeueMock() override = default;
};

TEST(TestRequeue, Initailization) {
  std::shared_ptr<RequeueMock> requeue = std::make_shared<RequeueMock>();
  requeue->fill();

  auto package = requeue->dequeue();

  ASSERT_EQ(package.is_empty(), false);
  ASSERT_EQ(package.data().get_data(), "package_4");
}

TEST(TestRequeue, RequeueBehavior) {
  std::shared_ptr<RequeueMock> requeue = std::make_shared<RequeueMock>();
  requeue->fill();
  {
    auto package = requeue->dequeue();

    ASSERT_EQ(package.is_empty(), false);
    ASSERT_EQ(package.data().get_data(), "package_4");
    ASSERT_EQ(requeue->count_empty(), 4);
  }
  ASSERT_EQ(requeue->count_empty(), 5);
}
