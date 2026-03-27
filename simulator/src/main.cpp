#include <chrono>
#include <csignal>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "common/message.hpp"
#include "common/socket_utils.hpp"
#include "common/time_utils.hpp"

namespace {
volatile std::sig_atomic_t g_stop = 0;
void handle_sigint(int) { g_stop = 1; }

void simulate_device(const std::string& id, const std::string& host, int port, int interval_ms) {
  int fd = common::connect_to_server(host, port);
  if (fd < 0) return;

  auto send_msg = [&](const std::string& type, common::Payload payload) {
    common::Message m{type, id, common::unix_ms(), std::move(payload)};
    common::send_line(fd, common::serialize(m));
  };

  send_msg("device_register", {{"hw", "STM32"}, {"fw", "sim"}});
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> type_dist(0, 2);
  uint64_t task_id = 0;

  while (!g_stop) {
    send_msg("device_heartbeat", {{"alive", "true"}});
    send_msg("device_status", {{"mode", "normal"}, {"alert", "false"}, {"sensor", std::to_string(type_dist(rng) * 33)}});
    std::string tt = "normal";
    int t = type_dist(rng);
    if (t == 0) tt = "realtime";
    if (t == 1) tt = "high_precision";
    send_msg("task_submit", {{"task_id", id + "_" + std::to_string(task_id++)}, {"task_type", tt}, {"input_ref", "sim_frame"}});
    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
  }
  common::close_fd(fd);
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 5) {
    std::cerr << "Usage: multi_device_simulator <cloud_host> <cloud_port> <device_count> <interval_ms>\n";
    return 1;
  }
  std::signal(SIGINT, handle_sigint);
  std::string host = argv[1];
  int port = std::stoi(argv[2]);
  int count = std::stoi(argv[3]);
  int interval_ms = std::stoi(argv[4]);

  std::vector<std::thread> threads;
  for (int i = 0; i < count; ++i) threads.emplace_back(simulate_device, "stm32_" + std::to_string(i), host, port, interval_ms);

  while (!g_stop) std::this_thread::sleep_for(std::chrono::seconds(1));
  for (auto& t : threads) if (t.joinable()) t.join();
  return 0;
}
