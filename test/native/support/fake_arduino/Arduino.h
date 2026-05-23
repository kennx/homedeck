#pragma once

#include <cstdint>

inline std::uint32_t gFakeMillis = 0;
inline void (*gFakeDelayCallback)(unsigned long) = nullptr;

// Native host keeps unsigned long at the platform ABI width (64-bit on Darwin),
// so this fake only constrains the stored clock value to 32 bits. It cannot make
// production expressions like millis() - start execute with Arduino's 32-bit math.
inline unsigned long millis() {
  return static_cast<unsigned long>(gFakeMillis);
}

inline void fakeArduinoSetMillis(unsigned long value) {
  gFakeMillis = static_cast<std::uint32_t>(value);
}

inline void fakeArduinoResetClock() {
  gFakeMillis = 0;
  gFakeDelayCallback = nullptr;
}

inline void delay(unsigned long ms) {
  if (gFakeDelayCallback != nullptr) {
    gFakeDelayCallback(ms);
    return;
  }

  gFakeMillis = static_cast<std::uint32_t>(gFakeMillis + static_cast<std::uint32_t>(ms));
}
