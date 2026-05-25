#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

class FakeFile {
 public:
  FakeFile() = default;

  explicit FakeFile(const std::vector<std::uint8_t>* data) : data_(data), open_(data != nullptr) {
  }

  std::size_t read(std::uint8_t* buffer, std::size_t length) {
    if (!open_ || data_ == nullptr || buffer == nullptr) {
      return 0;
    }
    const std::size_t remaining = data_->size() - std::min(position_, data_->size());
    const std::size_t count = std::min(length, remaining);
    if (count > 0) {
      std::memcpy(buffer, data_->data() + position_, count);
      position_ += count;
    }
    return count;
  }

  bool seek(std::uint32_t position) {
    if (!open_ || data_ == nullptr || position > data_->size()) {
      return false;
    }
    position_ = position;
    return true;
  }

  std::size_t size() const {
    return data_ != nullptr ? data_->size() : 0;
  }

  void close() {
    open_ = false;
    data_ = nullptr;
    position_ = 0;
  }

  explicit operator bool() const {
    return open_ && data_ != nullptr;
  }

 private:
  const std::vector<std::uint8_t>* data_ = nullptr;
  std::size_t position_ = 0;
  bool open_ = false;
};

using File = FakeFile;

class LittleFSClass {
 public:
  bool begin() {
    began = true;
    return beginSucceeds;
  }

  void end() {
    ended = true;
  }

  File open(const char* path, const char* mode = "r") {
    (void)mode;
    if (path == nullptr) {
      return {};
    }
    auto it = files.find(path);
    if (it == files.end()) {
      return {};
    }
    return File(&it->second);
  }

  bool exists(const char* path) const {
    return path != nullptr && files.find(path) != files.end();
  }

  bool beginSucceeds = true;
  bool began = false;
  bool ended = false;
  std::map<std::string, std::vector<std::uint8_t>> files;
};

inline LittleFSClass LittleFS;

inline void fakeLittleFSReset() {
  LittleFS.beginSucceeds = true;
  LittleFS.began = false;
  LittleFS.ended = false;
  LittleFS.files.clear();
}

inline void fakeLittleFSSetFile(const std::string& path, const std::vector<std::uint8_t>& data) {
  LittleFS.files[path] = data;
}
