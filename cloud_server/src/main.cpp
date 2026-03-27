#include "cloud/cloud_server.hpp"

#include <cstdlib>

int main(int argc, char** argv) {
  int port = 9000;
  if (argc > 1) port = std::atoi(argv[1]);
  cloud::CloudServer server(port);
  server.run();
  return 0;
}
