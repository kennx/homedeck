#pragma once

#include <cstdint>

#include "Arduino.h"

using esp_err_t = int;
using gpio_num_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_ERR_INVALID_ARG = 0x102;

enum esp_sleep_pd_domain_t {
  ESP_PD_DOMAIN_RTC_PERIPH = 0,
};

enum esp_sleep_pd_option_t {
  ESP_PD_OPTION_ON = 0,
};

enum esp_deepsleep_gpio_wake_up_mode_t {
  ESP_GPIO_WAKEUP_GPIO_LOW = 0,
  ESP_GPIO_WAKEUP_GPIO_HIGH = 1,
};

enum esp_sleep_wakeup_cause_t {
  ESP_SLEEP_WAKEUP_UNDEFINED = 0,
  ESP_SLEEP_WAKEUP_TIMER = 1,
};

inline esp_sleep_wakeup_cause_t gFakeWakeupCause = ESP_SLEEP_WAKEUP_UNDEFINED;
inline uint64_t gFakeSleepDurationUs = 0;
inline bool gDeepSleepCalled = false;
inline bool gGpioHoldCalled = false;
inline bool gFakeTimerWakeupConfigured = false;
inline bool gFakeGpioWakeupConfigured = false;
inline uint64_t gFakeGpioWakeupMask = 0;
inline esp_deepsleep_gpio_wake_up_mode_t gFakeGpioWakeupMode = ESP_GPIO_WAKEUP_GPIO_LOW;
inline esp_err_t gFakeTimerWakeupError = ESP_OK;
inline esp_err_t gFakeGpioWakeupError = ESP_OK;
inline bool gFakeSleepPdConfigured = false;
inline esp_sleep_pd_domain_t gFakeSleepPdDomain = ESP_PD_DOMAIN_RTC_PERIPH;
inline esp_sleep_pd_option_t gFakeSleepPdOption = ESP_PD_OPTION_ON;
inline esp_err_t gFakeSleepPdConfigError = ESP_OK;
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
  gFakeGpioWakeupConfigured = false;
  gFakeGpioWakeupMask = 0;
  gFakeGpioWakeupMode = ESP_GPIO_WAKEUP_GPIO_LOW;
  gFakeTimerWakeupError = ESP_OK;
  gFakeGpioWakeupError = ESP_OK;
  gFakeSleepPdConfigured = false;
  gFakeSleepPdDomain = ESP_PD_DOMAIN_RTC_PERIPH;
  gFakeSleepPdOption = ESP_PD_OPTION_ON;
  gFakeSleepPdConfigError = ESP_OK;
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
  gFakeGpioWakeupConfigured = false;
  gFakeGpioWakeupMask = 0;
  gFakeGpioWakeupMode = ESP_GPIO_WAKEUP_GPIO_LOW;
  gFakeTimerWakeupError = ESP_OK;
  gFakeGpioWakeupError = ESP_OK;
  gFakeSleepPdConfigured = false;
  gFakeSleepPdDomain = ESP_PD_DOMAIN_RTC_PERIPH;
  gFakeSleepPdOption = ESP_PD_OPTION_ON;
  gFakeSleepPdConfigError = ESP_OK;
}

inline std::uint8_t* fakeEspSleepRtcMemory() {
  return gFakeRtcMemory;
}

inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() {
  return gFakeWakeupCause;
}

inline esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us) {
  if (gFakeTimerWakeupError != ESP_OK) {
    return gFakeTimerWakeupError;
  }
  gFakeSleepDurationUs = time_in_us;
  gFakeTimerWakeupConfigured = true;
  return ESP_OK;
}

inline esp_err_t esp_deep_sleep_enable_gpio_wakeup(
    uint64_t gpio_pin_mask,
    esp_deepsleep_gpio_wake_up_mode_t mode) {
  if (gFakeGpioWakeupError != ESP_OK) {
    return gFakeGpioWakeupError;
  }
  gFakeGpioWakeupConfigured = true;
  gFakeGpioWakeupMask = gpio_pin_mask;
  gFakeGpioWakeupMode = mode;
  return ESP_OK;
}

inline int gFakeExt0Gpio = -1;
inline int gFakeExt0Level = 0;
inline esp_err_t gFakeExt0WakeupError = ESP_OK;

inline void fakeEspSleepResetExt0() {
  gFakeExt0Gpio = -1;
  gFakeExt0Level = 0;
  gFakeExt0WakeupError = ESP_OK;
}

inline esp_err_t esp_sleep_enable_ext0_wakeup(int gpio_num, int level) {
  if (gFakeExt0WakeupError != ESP_OK) {
    return gFakeExt0WakeupError;
  }
  gFakeExt0Gpio = gpio_num;
  gFakeExt0Level = level;
  return ESP_OK;
}

inline esp_err_t esp_sleep_pd_config(esp_sleep_pd_domain_t domain, esp_sleep_pd_option_t option) {
  if (gFakeSleepPdConfigError != ESP_OK) {
    return gFakeSleepPdConfigError;
  }
  gFakeSleepPdConfigured = true;
  gFakeSleepPdDomain = domain;
  gFakeSleepPdOption = option;
  return ESP_OK;
}

inline void esp_deep_sleep_start() {
  gDeepSleepCalled = true;
}

inline void gpio_deep_sleep_hold_en() {
  gGpioHoldCalled = true;
}
