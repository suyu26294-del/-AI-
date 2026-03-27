#include "common/thread_pool.hpp"

namespace common {

ThreadPool::ThreadPool(size_t n) {
  workers_.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    workers_.emplace_back([this] {
      while (true) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lk(mu_);
          cv_.wait(lk, [this] { return stop_ || !q_.empty(); });
          if (stop_ && q_.empty()) return;
          task = std::move(q_.front());
          q_.pop();
        }
        task();
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    stop_ = true;
  }
  cv_.notify_all();
  for (auto& worker : workers_) {
    if (worker.joinable()) worker.join();
  }
}

void ThreadPool::enqueue(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lk(mu_);
    q_.push(std::move(task));
  }
  cv_.notify_one();
}

}  // namespace common
