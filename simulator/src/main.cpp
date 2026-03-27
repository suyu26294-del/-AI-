#include <thread>
#include <vector>

#include "device/device_client.hpp"

int main(int argc, char** argv) {
  int num_devices = argc > 1 ? std::atoi(argv[1]) : 20;
  int port = argc > 2 ? std::atoi(argv[2]) : 9000;
  int seconds = argc > 3 ? std::atoi(argv[3]) : 20;

  std::vector<std::thread> threads;
  threads.reserve(num_devices);
  for (int i = 0; i < num_devices; ++i) {
    threads.emplace_back([i, port, seconds] {
      device::DeviceClient c("sim-device-" + std::to_string(i), "127.0.0.1", port);
      c.run(seconds);
    });
  }
  for (auto& t : threads) t.join();
  return 0;
}
