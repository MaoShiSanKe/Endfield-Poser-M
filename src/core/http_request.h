#pragma once
#include <string>
#include <cstddef>

namespace poser_http {
// One bounded request per connection; no chunked transfer or pipelining.
struct Request {
  enum class State { Waiting, Complete, Invalid };
  static constexpr size_t MaxHeader = 16 * 1024, MaxBody = 1024 * 1024;
  std::string bytes, method, path, body, error;
  size_t bodyStart = 0, bodySize = 0;
  bool parsed = false;
  State state = State::Waiting;
  State fail(const char* why) { error = why; return state = State::Invalid; }
  static std::string lower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return s;
  }
  static std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? std::string() : s.substr(a, b-a+1);
  }
  State append(const char* data, size_t count) {
    if (state != State::Waiting) return state;
    if (count > MaxHeader + MaxBody - bytes.size()) return fail("Request too large");
    bytes.append(data, count);
    if (!parsed) {
      auto end = bytes.find("\r\n\r\n");
      if (end == std::string::npos) {
        if (bytes.size() >= MaxHeader) return fail("Headers too large");
        return state;
      }
      bodyStart = end + 4;
      if (bodyStart > MaxHeader) return fail("Headers too large");
      auto line = bytes.find("\r\n"), a = bytes.find(' ');
      if (a == std::string::npos || a >= line) return fail("Invalid request line");
      auto b = bytes.find(' ', a+1);
      if (b == std::string::npos || b >= line || b == a+1) return fail("Invalid request line");
      method = bytes.substr(0,a); path = bytes.substr(a+1,b-a-1);
      auto version = bytes.substr(b+1,line-b-1);
      if (version != "HTTP/1.1" && version != "HTTP/1.0") return fail("Invalid HTTP version");
      bool hasLength = false;
      for (size_t p = line+2; p < end;) {
        auto next = bytes.find("\r\n",p), colon = bytes.find(':',p);
        if (colon == std::string::npos || colon >= next || colon == p) return fail("Invalid header");
        auto name = lower(bytes.substr(p,colon-p));
        auto value = trim(bytes.substr(colon+1,next-colon-1));
        if (name == "transfer-encoding") return fail("Transfer encoding unsupported");
        if (name == "content-length") {
          if (hasLength || value.empty()) return fail("Invalid Content-Length");
          hasLength = true;
          for (char c : value) {
            if (c < '0' || c > '9') return fail("Invalid Content-Length");
            size_t digit = size_t(c-'0');
            if (bodySize > (MaxBody-digit)/10) return fail("Body too large");
            bodySize = bodySize*10+digit;
          }
        }
        p = next+2;
      }
      if ((method == "POST" || method == "PUT" || method == "PATCH") && !hasLength)
        return fail("Content-Length required");
      parsed = true;
    }
    if (bytes.size()-bodyStart < bodySize) return state;
    body = bytes.substr(bodyStart,bodySize);
    return state = State::Complete;
  }
};
}
