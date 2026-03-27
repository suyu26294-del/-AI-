#include <csignal>
#include <iostream>
#include <thread>

#include "cloud/cloud_server.hpp"

namespace {
volatile std::sig_atomic_t g_stop = 0;
void handle_sigint(int) { g_stop = 1; }
}  // namespace

int main(int argc, char** argv) {
  int port = 9000;
  if (argc > 1) {
    port = std::stoi(argv[1]);
  }

  std::signal(SIGINT, handle_sigint);
  cloud::CloudServer server(port);
  if (!server.start()) {
    std::cerr << "failed to start cloud server\n";
    return 1;
  }

  while (!g_stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  server.stop();
  return 0;
}
