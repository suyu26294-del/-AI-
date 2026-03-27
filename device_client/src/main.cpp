#include <csignal>
#include <iostream>
#include <thread>

#include "device/device_client.hpp"

namespace {
volatile std::sig_atomic_t g_stop = 0;
void handle_sigint(int) { g_stop = 1; }
}

int main(int argc, char** argv) {
  if (argc < 4) {
    std::cerr << "Usage: device_client <device_id> <cloud_host> <cloud_port> [submit_interval_ms]\n";
    return 1;
  }

  std::string device_id = argv[1];
  std::string host = argv[2];
  int port = std::stoi(argv[3]);
  int submit_interval_ms = argc >= 5 ? std::stoi(argv[4]) : 3000;

  std::signal(SIGINT, handle_sigint);

  device::DeviceClient client(device_id, host, port, submit_interval_ms);
  if (!client.start()) {
    std::cerr << "device client failed to connect\n";
    return 1;
  }

  while (!g_stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  client.stop();
  return 0;
}
