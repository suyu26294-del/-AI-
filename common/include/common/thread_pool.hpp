#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace common {

class ThreadPool {
 public:
  explicit ThreadPool(size_t n);
  ~ThreadPool();

  void enqueue(std::function<void()> task);

 private:
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> q_;
  std::mutex mu_;
  std::condition_variable cv_;
  bool stop_{false};
};

}  // namespace common
