#include "common/socket_utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace common {

TcpConnection::TcpConnection(int fd) : fd_(fd) {}
TcpConnection::~TcpConnection() { close(); }

TcpConnection::TcpConnection(TcpConnection&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept {
  if (this != &other) {
    close();
    fd_ = other.fd_;
    other.fd_ = -1;
  }
  return *this;
}

bool TcpConnection::valid() const { return fd_ >= 0; }
int TcpConnection::fd() const { return fd_; }

void TcpConnection::close() {
  if (fd_ >= 0) {
    ::shutdown(fd_, SHUT_RDWR);
    ::close(fd_);
    fd_ = -1;
  }
}

bool TcpConnection::send_line(const std::string& line) {
  std::lock_guard<std::mutex> lk(write_mu_);
  if (fd_ < 0) return false;
  const char* ptr = line.data();
  ssize_t left = static_cast<ssize_t>(line.size());
  while (left > 0) {
    ssize_t sent = ::send(fd_, ptr, left, 0);
    if (sent <= 0) return false;
    ptr += sent;
    left -= sent;
  }
  return true;
}

std::optional<std::string> TcpConnection::recv_line() {
  if (fd_ < 0) return std::nullopt;
  std::string out;
  char c{};
  while (true) {
    ssize_t n = ::recv(fd_, &c, 1, 0);
    if (n <= 0) return std::nullopt;
    if (c == '\n') return out;
    out.push_back(c);
  }
}

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
    ::close(fd);
    return -1;
  }
  if (listen(fd, backlog) < 0) {
    ::close(fd);
    return -1;
  }
  return fd;
}

TcpConnection accept_client(int server_fd) {
  sockaddr_in cli{};
  socklen_t len = sizeof(cli);
  int cfd = ::accept(server_fd, reinterpret_cast<sockaddr*>(&cli), &len);
  return TcpConnection(cfd);
}

TcpConnection connect_to_server(const std::string& host, int port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return TcpConnection{};
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
    ::close(fd);
    return TcpConnection{};
  }
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return TcpConnection{};
  }
  return TcpConnection(fd);
}

}  // namespace common
