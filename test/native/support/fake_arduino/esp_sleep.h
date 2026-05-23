#pragma once

#include <cstdint>

#include "Arduino.h"

enum esp_sleep_wakeup_cause_t {
  ESP_SLEEP_WAKEUP_UNDEFINED = 0,
  ESP_SLEEP_WAKEUP_TIMER = 1,
};

inline esp_sleep_wakeup_cause_t gFakeWakeupCause = ESP_SLEEP_WAKEUP_UNDEFINED;
inline uint64_t gFakeSleepDurationUs = 0;
inline bool gDeepSleepCalled = false;
inline bool gGpioHoldCalled = false;
inline bool gFakeTimerWakeupConfigured = false;
inline std::uint8_t gFakeRtcMemory[512] = {};

inline void fakeEspSleepSetWakeupCause(esp_sleep_wakeup_cause_t cause) {
  gFakeWakeupCause = cause;
}

inline void fakeEspSleepReset() {
  gFakeWakeupCause = ESP_SLEEP_WAKEUP_UNDEFINED;
  gFakeSleepDurationUs = 0;
  gDeepSleepCalled = false;
  gGpioHoldCalled = false;
  gFakeTimerWakeupConfigured = false;
  for (auto& byte : gFakeRtcMemory) {
    byte = 0;
  }
}

inline void fakeEspSleepReboot() {
  fakeArduinoResetClock();
  gFakeWakeupCause = (gDeepSleepCalled && gFakeTimerWakeupConfigured)
      ? ESP_SLEEP_WAKEUP_TIMER
      : ESP_SLEEP_WAKEUP_UNDEFINED;
  gFakeSleepDurationUs = 0;
  gDeepSleepCalled = false;
  gGpioHoldCalled = false;
  gFakeTimerWakeupConfigured = false;
}

inline std::uint8_t* fakeEspSleepRtcMemory() {
  return gFakeRtcMemory;
}

inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() {
  return gFakeWakeupCause;
}

inline void esp_sleep_enable_timer_wakeup(uint64_t time_in_us) {
  gFakeSleepDurationUs = time_in_us;
  gFakeTimerWakeupConfigured = true;
}

inline void esp_deep_sleep_start() {
  gDeepSleepCalled = true;
}

inline void gpio_deep_sleep_hold_en() {
  gGpioHoldCalled = true;
}
