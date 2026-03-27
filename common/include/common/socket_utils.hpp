#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace common {

class TcpConnection {
 public:
  explicit TcpConnection(int fd = -1);
  ~TcpConnection();
  TcpConnection(const TcpConnection&) = delete;
  TcpConnection& operator=(const TcpConnection&) = delete;
  TcpConnection(TcpConnection&& other) noexcept;
  TcpConnection& operator=(TcpConnection&& other) noexcept;

  bool valid() const;
  int fd() const;
  void close();
  bool send_line(const std::string& line);
  std::optional<std::string> recv_line();

 private:
  int fd_;
  std::mutex write_mu_;
};

int create_server_socket(int port, int backlog = 128);
TcpConnection accept_client(int server_fd);
TcpConnection connect_to_server(const std::string& host, int port);

}  // namespace common
