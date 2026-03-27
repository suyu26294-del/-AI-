#pragma once

#include <string>

namespace common {

int create_server_socket(int port, int backlog = 128);
int connect_to_server(const std::string& host, int port);
bool send_line(int fd, const std::string& line);
bool recv_line(int fd, std::string& out_line);
void close_fd(int fd);

}  // namespace common
