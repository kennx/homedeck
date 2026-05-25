#pragma once

#include <esp_sleep.h>

enum rtc_gpio_mode_t {
  RTC_GPIO_MODE_INPUT_ONLY = 0,
};

inline int gFakeRtcGpioPullupPin = -1;
inline int gFakeRtcGpioPulldownDisabledPin = -1;
inline int gFakeRtcGpioSleepDirectionPin = -1;
inline rtc_gpio_mode_t gFakeRtcGpioSleepDirectionMode = RTC_GPIO_MODE_INPUT_ONLY;
inline esp_err_t gFakeRtcGpioPullupError = ESP_OK;
inline esp_err_t gFakeRtcGpioPulldownDisableError = ESP_OK;
inline esp_err_t gFakeRtcGpioSleepDirectionError = ESP_OK;

inline void fakeRtcIoReset() {
  gFakeRtcGpioPullupPin = -1;
  gFakeRtcGpioPulldownDisabledPin = -1;
  gFakeRtcGpioSleepDirectionPin = -1;
  gFakeRtcGpioSleepDirectionMode = RTC_GPIO_MODE_INPUT_ONLY;
  gFakeRtcGpioPullupError = ESP_OK;
  gFakeRtcGpioPulldownDisableError = ESP_OK;
  gFakeRtcGpioSleepDirectionError = ESP_OK;
}

inline esp_err_t rtc_gpio_pullup_en(gpio_num_t gpio_num) {
  if (gFakeRtcGpioPullupError != ESP_OK) {
    return gFakeRtcGpioPullupError;
  }
  gFakeRtcGpioPullupPin = gpio_num;
  return ESP_OK;
}

inline esp_err_t rtc_gpio_pulldown_dis(gpio_num_t gpio_num) {
  if (gFakeRtcGpioPulldownDisableError != ESP_OK) {
    return gFakeRtcGpioPulldownDisableError;
  }
  gFakeRtcGpioPulldownDisabledPin = gpio_num;
  return ESP_OK;
}

inline esp_err_t rtc_gpio_set_direction_in_sleep(gpio_num_t gpio_num, rtc_gpio_mode_t mode) {
  if (gFakeRtcGpioSleepDirectionError != ESP_OK) {
    return gFakeRtcGpioSleepDirectionError;
  }
  gFakeRtcGpioSleepDirectionPin = gpio_num;
  gFakeRtcGpioSleepDirectionMode = mode;
  return ESP_OK;
}
