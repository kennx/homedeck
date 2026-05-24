#pragma once

#include <functional>
#include <map>
#include <string>

constexpr int HTTP_GET = 0;
constexpr int HTTP_POST = 1;

class WebServer {
 public:
  explicit WebServer(int port) : port_(port) {}

  void on(const char* path, int, std::function<void()> handler) {
    handlers_[path] = std::move(handler);
  }

  void begin() { started_ = true; }
  void handleClient() {}
  std::string arg(const char* name) const {
    const auto found = args_.find(name);
    return found == args_.end() ? std::string{} : found->second;
  }
  void send(int status, const char* type, const char* body) {
    lastStatus = status;
    lastType = type != nullptr ? type : "";
    lastBody = body != nullptr ? body : "";
  }

  int port_ = 80;
  bool started_ = false;
  int lastStatus = 0;
  std::string lastType;
  std::string lastBody;
  std::map<std::string, std::string> args_;
  std::map<std::string, std::function<void()>> handlers_;
};
