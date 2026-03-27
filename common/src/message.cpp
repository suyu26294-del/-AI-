#include "common/message.hpp"

#include <charconv>
#include <sstream>

namespace common {

namespace {
std::string escape_json(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else out.push_back(c);
  }
  return out;
}

void skip_ws(const std::string& s, size_t& i) {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
}

bool parse_string(const std::string& s, size_t& i, std::string& out) {
  skip_ws(s, i);
  if (i >= s.size() || s[i] != '"') return false;
  ++i;
  out.clear();
  while (i < s.size()) {
    char c = s[i++];
    if (c == '"') return true;
    if (c == '\\' && i < s.size()) {
      char n = s[i++];
      if (n == 'n') out.push_back('\n');
      else out.push_back(n);
      continue;
    }
    out.push_back(c);
  }
  return false;
}

bool parse_literal_value(const std::string& s, size_t& i, std::string& out) {
  skip_ws(s, i);
  if (i >= s.size()) return false;
  if (s[i] == '"') return parse_string(s, i, out);
  size_t start = i;
  while (i < s.size() && s[i] != ',' && s[i] != '}' && s[i] != '\n' && s[i] != '\r') ++i;
  out = s.substr(start, i - start);
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return !out.empty();
}

bool consume(const std::string& s, size_t& i, char c) {
  skip_ws(s, i);
  if (i >= s.size() || s[i] != c) return false;
  ++i;
  return true;
}

bool parse_payload_object(const std::string& s, size_t& i, Payload& payload) {
  payload.clear();
  if (!consume(s, i, '{')) return false;
  skip_ws(s, i);
  if (i < s.size() && s[i] == '}') {
    ++i;
    return true;
  }

  while (i < s.size()) {
    std::string key;
    if (!parse_string(s, i, key)) return false;
    if (!consume(s, i, ':')) return false;
    std::string val;
    if (!parse_literal_value(s, i, val)) return false;
    payload[key] = val;
    skip_ws(s, i);
    if (i < s.size() && s[i] == ',') {
      ++i;
      continue;
    }
    if (i < s.size() && s[i] == '}') {
      ++i;
      return true;
    }
    return false;
  }
  return false;
}

bool parse_top_object(const std::string& s, Message& m) {
  size_t i = 0;
  if (!consume(s, i, '{')) return false;

  bool has_type = false, has_source = false, has_ts = false, has_payload = false;
  while (i < s.size()) {
    std::string key;
    if (!parse_string(s, i, key)) return false;
    if (!consume(s, i, ':')) return false;

    if (key == "payload") {
      if (!parse_payload_object(s, i, m.payload)) return false;
      has_payload = true;
    } else {
      std::string value;
      if (!parse_literal_value(s, i, value)) return false;
      if (key == "type") {
        m.type = value;
        has_type = true;
      } else if (key == "source_id") {
        m.source_id = value;
        has_source = true;
      } else if (key == "timestamp") {
        try {
          m.timestamp_ms = std::stoll(value);
          has_ts = true;
        } catch (...) {
          return false;
        }
      }
    }

    skip_ws(s, i);
    if (i < s.size() && s[i] == ',') {
      ++i;
      continue;
    }
    if (i < s.size() && s[i] == '}') {
      ++i;
      break;
    }
  }

  return has_type && has_source && has_ts && has_payload;
}
}  // namespace

std::string serialize(const Message& m) {
  std::ostringstream os;
  os << "{\"type\":\"" << escape_json(m.type) << "\",";
  os << "\"source_id\":\"" << escape_json(m.source_id) << "\",";
  os << "\"timestamp\":" << m.timestamp_ms << ",";
  os << "\"payload\":{";
  bool first = true;
  for (const auto& [k, v] : m.payload) {
    if (!first) os << ",";
    first = false;
    os << "\"" << escape_json(k) << "\":\"" << escape_json(v) << "\"";
  }
  os << "}}";
  return os.str();
}

std::optional<Message> parse_message(const std::string& raw) {
  Message m;
  if (!parse_top_object(raw, m)) return std::nullopt;
  return m;
}

std::string payload_get(const Payload& p, const std::string& key, const std::string& def) {
  auto it = p.find(key);
  return it == p.end() ? def : it->second;
}

int payload_get_int(const Payload& p, const std::string& key, int def) {
  auto it = p.find(key);
  if (it == p.end()) return def;
  try {
    return std::stoi(it->second);
  } catch (...) {
    return def;
  }
}

double payload_get_double(const Payload& p, const std::string& key, double def) {
  auto it = p.find(key);
  if (it == p.end()) return def;
  try {
    return std::stod(it->second);
  } catch (...) {
    return def;
  }
}

bool payload_get_bool(const Payload& p, const std::string& key, bool def) {
  auto it = p.find(key);
  if (it == p.end()) return def;
  return it->second == "true" || it->second == "1";
}

}  // namespace common
