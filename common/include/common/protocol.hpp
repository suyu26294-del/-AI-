#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace common {

using Payload = std::unordered_map<std::string, std::string>;

struct Message {
  std::string type;
  std::string source_id;
  int64_t timestamp{0};
  Payload payload;
};

std::string make_message(const std::string& type, const std::string& source_id, const Payload& payload = {});
std::optional<Message> decode_message(const std::string& line);

std::string payload_get(const Payload& payload, const std::string& key, const std::string& def = "");
int payload_get_int(const Payload& payload, const std::string& key, int def = 0);
double payload_get_double(const Payload& payload, const std::string& key, double def = 0.0);

}  // namespace common
