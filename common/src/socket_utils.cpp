#include "common/socket_utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

namespace common {

int create_server_socket(int port, int backlog) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  int opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);
  if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(fd);
    return -1;
  }
  if (listen(fd, backlog) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

int connect_to_server(const std::string& host, int port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
    close(fd);
    return -1;
  }

  if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

bool send_line(int fd, const std::string& line) {
  std::string data = line + "\n";
  const char* p = data.data();
  size_t left = data.size();
  while (left > 0) {
    ssize_t n = send(fd, p, left, 0);
    if (n <= 0) return false;
    p += n;
    left -= static_cast<size_t>(n);
  }
  return true;
}

bool recv_line(int fd, std::string& out_line) {
  out_line.clear();
  char c;
  while (true) {
    ssize_t n = recv(fd, &c, 1, 0);
    if (n <= 0) return false;
    if (c == '\n') break;
    out_line.push_back(c);
  }
  return true;
}

void close_fd(int fd) {
  if (fd >= 0) {
    shutdown(fd, SHUT_RDWR);
    close(fd);
  }
}

}  // namespace common
