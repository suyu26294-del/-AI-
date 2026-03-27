#include "common/protocol.hpp"

#include <sstream>

#include "common/types.hpp"

namespace common {

namespace {

std::string escape_json(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    if (c == '\\' || c == '"') out.push_back('\\');
    out.push_back(c);
  }
  return out;
}

std::optional<std::string> extract_quoted(const std::string& src, const std::string& key_pattern, size_t from = 0) {
  auto pos = src.find(key_pattern, from);
  if (pos == std::string::npos) return std::nullopt;
  auto q1 = src.find('"', pos + key_pattern.size());
  if (q1 == std::string::npos) return std::nullopt;
  auto q2 = src.find('"', q1 + 1);
  if (q2 == std::string::npos) return std::nullopt;
  return src.substr(q1 + 1, q2 - q1 - 1);
}

std::optional<int64_t> extract_number(const std::string& src, const std::string& key_pattern) {
  auto pos = src.find(key_pattern);
  if (pos == std::string::npos) return std::nullopt;
  auto start = src.find(':', pos);
  if (start == std::string::npos) return std::nullopt;
  start++;
  while (start < src.size() && src[start] == ' ') start++;
  size_t end = start;
  while (end < src.size() && (isdigit(src[end]) || src[end] == '-')) end++;
  if (end == start) return std::nullopt;
  return std::stoll(src.substr(start, end - start));
}

}  // namespace

std::string make_message(const std::string& type, const std::string& source_id, const Payload& payload) {
  std::ostringstream oss;
  oss << "{\"type\":\"" << escape_json(type) << "\",\"source_id\":\"" << escape_json(source_id)
      << "\",\"timestamp\":" << now_ms() << ",\"payload\":{";
  bool first = true;
  for (const auto& [k, v] : payload) {
    if (!first) oss << ',';
    first = false;
    oss << "\"" << escape_json(k) << "\":\"" << escape_json(v) << "\"";
  }
  oss << "}}\n";
  return oss.str();
}

std::optional<Message> decode_message(const std::string& line) {
  Message msg;
  auto type = extract_quoted(line, "\"type\":");
  auto source = extract_quoted(line, "\"source_id\":");
  auto ts = extract_number(line, "\"timestamp\"");
  auto payload_pos = line.find("\"payload\":{");
  if (!type || !source || !ts || payload_pos == std::string::npos) return std::nullopt;
  msg.type = *type;
  msg.source_id = *source;
  msg.timestamp = *ts;

  auto start = payload_pos + std::string("\"payload\":{").size();
  auto end = line.find('}', start);
  if (end == std::string::npos) return std::nullopt;
  std::string body = line.substr(start, end - start);

  size_t i = 0;
  while (i < body.size()) {
    auto k1 = body.find('"', i);
    if (k1 == std::string::npos) break;
    auto k2 = body.find('"', k1 + 1);
    if (k2 == std::string::npos) break;
    auto c = body.find(':', k2);
    if (c == std::string::npos) break;
    auto v1 = body.find('"', c);
    if (v1 == std::string::npos) break;
    auto v2 = body.find('"', v1 + 1);
    if (v2 == std::string::npos) break;
    msg.payload[body.substr(k1 + 1, k2 - k1 - 1)] = body.substr(v1 + 1, v2 - v1 - 1);
    i = v2 + 1;
  }
  return msg;
}

std::string payload_get(const Payload& payload, const std::string& key, const std::string& def) {
  auto it = payload.find(key);
  return it == payload.end() ? def : it->second;
}

int payload_get_int(const Payload& payload, const std::string& key, int def) {
  auto it = payload.find(key);
  if (it == payload.end()) return def;
  try {
    return std::stoi(it->second);
  } catch (...) {
    return def;
  }
}

double payload_get_double(const Payload& payload, const std::string& key, double def) {
  auto it = payload.find(key);
  if (it == payload.end()) return def;
  try {
    return std::stod(it->second);
  } catch (...) {
    return def;
  }
}

}  // namespace common
