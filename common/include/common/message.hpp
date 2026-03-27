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
  int64_t timestamp_ms{};
  Payload payload;
};

std::string serialize(const Message& m);
std::optional<Message> parse_message(const std::string& raw);

std::string payload_get(const Payload& p, const std::string& key, const std::string& def = "");
int payload_get_int(const Payload& p, const std::string& key, int def = 0);
double payload_get_double(const Payload& p, const std::string& key, double def = 0.0);
bool payload_get_bool(const Payload& p, const std::string& key, bool def = false);

}  // namespace common
