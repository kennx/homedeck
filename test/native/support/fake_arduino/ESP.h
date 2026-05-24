#pragma once

#include <cstdint>

struct FakeESPClass {
  bool restartCalled = false;
  std::uint64_t efuseMac = 0x1234ABCD;
  void restart() { restartCalled = true; }
  std::uint64_t getEfuseMac() const { return efuseMac; }
};

inline FakeESPClass ESP;
