#include "app/app_runtime.h"

#include <Arduino.h>
#include <ESP.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <driver/rtc_io.h>
#include <esp_sntp.h>
#include <esp_sleep.h>
#include <time.h>

#include <Adafruit_NeoPixel.h>
#include <M5PM1.h>
#ifndef UNIT_TEST
#include <driver/gpio.h>
#include <esp_rom_gpio.h>
#include <esp_attr.h>
#endif

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>

#include "app/boot_controller.h"
#include "config/config_portal.h"
#include "config/config_store.h"
#include "views/almanac_view.h"
#include "views/calendar_view.h"
#include "views/countdown_view.h"
#include "views/home_renderer.h"
#include "system/sht40_reader.h"
#include "system/time_service.h"
#include "providers/timezone_catalog.h"
#include "system/wifi_connection.h"

namespace homedeck {

#ifdef UNIT_TEST
SystemView gRtcSavedView = SystemView::Almanac;
#else
RTC_DATA_ATTR SystemView gRtcSavedView = SystemView::Almanac;
#endif

namespace {

#ifndef UNIT_TEST
void i2cBusRecovery() {
  constexpr gpio_num_t kSclPin = GPIO_NUM_2;
  constexpr gpio_num_t kSdaPin = GPIO_NUM_3;

  gpio_config_t cfg;
  cfg.pin_bit_mask = (1ULL << kSclPin) | (1ULL << kSdaPin);
  cfg.mode = GPIO_MODE_INPUT_OUTPUT_OD;
  cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  cfg.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&cfg);

  gpio_set_level(kSdaPin, 1);
  gpio_set_level(kSclPin, 1);
  esp_rom_delay_us(10);

  for (int i = 0; i < 9; ++i) {
    gpio_set_level(kSclPin, 0);
    esp_rom_delay_us(5);
    gpio_set_level(kSclPin, 1);
    esp_rom_delay_us(5);
    if (gpio_get_level(kSdaPin) == 1) {
      break;
    }
  }

  gpio_set_level(kSdaPin, 0);
  esp_rom_delay_us(5);
  gpio_set_level(kSclPin, 1);
  esp_rom_delay_us(5);
  gpio_set_level(kSdaPin, 1);
  esp_rom_delay_us(5);

  gpio_reset_pin(kSclPin);
  gpio_reset_pin(kSdaPin);
}
#endif

constexpr uint8_t kRgbLedPin = 21;
constexpr uint8_t kRgbLedCount = 2;

M5PM1 gPm1;
Adafruit_NeoPixel gPixels(kRgbLedCount, kRgbLedPin, NEO_GRB + NEO_KHZ800);
bool gPm1Ready = false;

void clearRgbLedPixels() {
  gPixels.clear();
  gPixels.show();
}

void disableRgbLedPower() {
  if (gPm1Ready) {
    gPm1.setLdoEnable(false);
  }
}

void shutdownRgbLedForSleep() {
  clearRgbLedPixels();
  disableRgbLedPower();
}

void initRgbLed() {
  m5pm1_err_t err = gPm1.begin(&M5.In_I2C, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K);
  gPm1Ready = (err == M5PM1_OK);
  if (gPm1Ready) {
    gPm1.setLdoEnable(true);
  }
  gPixels.begin();
  clearRgbLedPixels();
}

void prepareEpdAfterWakeup() {
  M5.Display.setEpdMode(epd_mode_t::epd_quality);
  M5.Display.wakeup();
  M5.Display.setEpdMode(epd_mode_t::epd_fast);
}

#ifndef UNIT_TEST
void prepareEpdAfterDeepSleep() {
  M5.Display.setEpdMode(epd_mode_t::epd_fast);
  M5.Display.wakeup();
}
#endif

ConfigStore gConfigStore;
HomeRenderer gHomeRenderer;
AlmanacView gAlmanacView;
CalendarView gCalendarView;
CountdownView gCountdownView;
ConfigPortal gConfigPortal;
std::unique_ptr<TimeService> gTimeService;
std::unique_ptr<BootController> gBootController;

constexpr time_t kTrustedUnixTimeThreshold = 1704067200;
constexpr unsigned long kNtpSyncTimeoutMs = 10000;
constexpr unsigned long kNtpPollIntervalMs = 250;

std::string makeApSsid() {
  const std::uint64_t mac = ESP.getEfuseMac();
  char suffix[5] = {};
  std::snprintf(suffix, sizeof(suffix), "%04X", static_cast<unsigned int>(mac & 0xFFFF));
  return std::string("HomeDeck-") + suffix;
}

std::string softApIpAddress() {
  return WiFi.softAPIP().toString().c_str();
}

bool syncNtp(const std::string& posixTimezone, const std::string& ntpServer, time_t* syncedUnix) {
  if (syncedUnix == nullptr) {
    return false;
  }
  sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
  configTzTime(posixTimezone.c_str(), ntpServer.c_str());
  const unsigned long startedAt = millis();
  while (millis() - startedAt < kNtpSyncTimeoutMs) {
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
      const time_t now = time(nullptr);
      if (now >= kTrustedUnixTimeThreshold) {
        *syncedUnix = now;
        return true;
      }
    }
    delay(kNtpPollIntervalMs);
  }
  return false;
}

bool writeRtcUtc(time_t unixTime) {
  if (!M5.Rtc.isEnabled()) {
    return false;
  }
  const std::tm* utc = gmtime(&unixTime);
  if (utc == nullptr) {
    return false;
  }
  M5.Rtc.setDateTime(utc);

  // 验证写入：立即读取 RTC 并比对，防止 I2C 静默失败
  m5::rtc_datetime_t dt;
  if (M5.Rtc.getDateTime(&dt)) {
    std::tm readBack{};
    readBack.tm_year = dt.date.year - 1900;
    readBack.tm_mon = dt.date.month - 1;
    readBack.tm_mday = dt.date.date;
    readBack.tm_hour = dt.time.hours;
    readBack.tm_min = dt.time.minutes;
    readBack.tm_sec = dt.time.seconds;
    readBack.tm_isdst = 0;
    const char* previousTimezone = std::getenv("TZ");
    const std::string savedTimezone = previousTimezone != nullptr ? previousTimezone : "";
    setenv("TZ", "UTC0", 1);
    tzset();
    const time_t readBackUnix = std::mktime(&readBack);
    if (previousTimezone != nullptr) {
      setenv("TZ", savedTimezone.c_str(), 1);
    } else {
      unsetenv("TZ");
    }
    tzset();
    if (readBackUnix >= 0 && std::llabs(static_cast<long long>(readBackUnix) - static_cast<long long>(unixTime)) <= 1) {
      return true;
    }
  }
  return false;
}

TimeServiceDeps makeTimeDeps() {
  TimeServiceDeps deps{};
  deps.connectWifi = [](const std::string& ssid, const std::string& password) {
    return connectWifiPreservingAccessPoint(ssid, password);
  };
  deps.syncNtp = syncNtp;
  deps.writeRtcUtc = writeRtcUtc;
  deps.rtcAvailable = []() { return M5.Rtc.isEnabled(); };
  deps.rtcVoltLow = []() { return M5.Rtc.getVoltLow(); };
  deps.restoreSystemTimeFromRtc = []() { M5.Rtc.setSystemTimeFromRtc(); };
  return deps;
}

ConfigValidationResult saveSubmittedConfig(
    const SetupConfig& config,
    const ManualDateTime& manualDateTime) {
  const TimeCalibrationResult timeResult = gTimeService->calibrateOnSave(config, manualDateTime);
  if (!timeResult.ok()) {
    return ConfigValidationResult{ConfigValidationError::InvalidManualDateTime, timeResult.message};
  }
  if (!gConfigStore.saveSetupConfig(config) || !gConfigStore.saveConfigured(true)) {
    return ConfigValidationResult{ConfigValidationError::InvalidManualDateTime, "保存配置失败。"};
  }
  return ConfigValidationResult{};
}

void renderHomeWithEnvironment() {
  gAlmanacView.render();
}

void renderCalendarWithEnvironment() {
  gCalendarView.render();
}

void renderCalendarWithOffset(int monthOffset) {
  gCalendarView.renderWithOffset(monthOffset);
}

void renderAlmanacWithOffset(int dayOffset) {
  gAlmanacView.renderWithOffset(dayOffset);
}

}  // namespace

#ifdef UNIT_TEST
bool syncNtpForTest(
    const std::string& posixTimezone,
    const std::string& ntpServer,
    time_t* syncedUnix) {
  return syncNtp(posixTimezone, ntpServer, syncedUnix);
}

bool writeRtcUtcForTest(time_t unixTime) {
  return writeRtcUtc(unixTime);
}

void prepareEpdAfterWakeupForTest() {
  prepareEpdAfterWakeup();
}

void initRgbLedForTest() {
  initRgbLed();
}

void shutdownRgbLedForSleepForTest() {
  shutdownRgbLedForSleep();
}
#endif

void enterHomeDeepSleep(const HomeSleepRequest& request) {
  shutdownRgbLedForSleep();
  const auto wakeupGpio = static_cast<gpio_num_t>(request.wakeupGpio);
  pinMode(static_cast<std::uint8_t>(request.wakeupGpio), INPUT_PULLUP);
  if (esp_sleep_enable_timer_wakeup(request.timerWakeupUs) != ESP_OK) {
    return;
  }
  if (rtc_gpio_pullup_en(wakeupGpio) != ESP_OK) {
    return;
  }
  if (rtc_gpio_pulldown_dis(wakeupGpio) != ESP_OK) {
    return;
  }
  if (rtc_gpio_set_direction_in_sleep(wakeupGpio, RTC_GPIO_MODE_INPUT_ONLY) != ESP_OK) {
    return;
  }
  if (esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON) != ESP_OK) {
    return;
  }
  if (esp_sleep_enable_ext0_wakeup(wakeupGpio, request.wakeOnLow ? 0 : 1) != ESP_OK) {
    return;
  }
  M5.Display.sleep();
  M5.Display.waitDisplay();
  M5.Power.deepSleep(request.timerWakeupUs, false);
}

namespace {

BootControllerDeps makeBootDeps() {
  BootControllerDeps deps{};
  deps.loadFlags = []() { return gConfigStore.loadBootFlags(); };
  deps.clearForceConfigOnNextBoot = []() { return gConfigStore.clearForceConfigOnNextBoot(); };
  deps.setForceConfigOnNextBoot = []() { return gConfigStore.setForceConfigOnNextBoot(true); };
  deps.startConfigPortal = []() {
    const std::string apSsid = makeApSsid();
    gConfigPortal.begin(apSsid, gConfigStore.loadSetupConfig(), saveSubmittedConfig);
    gHomeRenderer.renderConfigPortal(apSsid, softApIpAddress());
  };
  deps.handleConfigPortalClient = []() { gConfigPortal.handleClient(); };
  deps.restoreSystemTimeFromRtc = []() {
    const SetupConfig config = gConfigStore.loadSetupConfig();
    if (!gTimeService->applyTimezone(config.timezoneIana)) {
      gTimeService->applyTimezone(defaultTimezone()->iana);
    }
    gTimeService->restoreSystemTimeFromRtc();
    // M5Unified 的 setSystemTimeFromRtc() 有 getenv/setenv 悬空指针 bug，
    // 会将 TZ 错误地恢复为 GMT0。必须在调用后重新应用时区。
    if (!gTimeService->applyTimezone(config.timezoneIana)) {
      gTimeService->applyTimezone(defaultTimezone()->iana);
    }
  };
  deps.renderAlmanac = renderHomeWithEnvironment;
  deps.renderCalendar = renderCalendarWithEnvironment;
  deps.renderCountdown = []() {
    gCountdownView.render();
  };
  deps.renderCalendarWithOffset = renderCalendarWithOffset;
  deps.renderAlmanacWithOffset = renderAlmanacWithOffset;
  deps.getCalendarButtonClickCount = []() -> int {
    if (M5.BtnC.wasDecideClickCount()) {
      return static_cast<int>(M5.BtnC.getClickCount());
    }
    return 0;
  };
  deps.wasPrevMonthClicked = []() { return M5.BtnB.wasClicked(); };
  deps.wasNextMonthClicked = []() { return M5.BtnA.wasClicked(); };
  deps.updateButtons = []() { M5.update(); };
  deps.areSetupButtonsPressed = []() { return M5.BtnA.isPressed() && M5.BtnB.isPressed(); };
  deps.millis = []() { return millis(); };
  deps.restart = []() { ESP.restart(); };
  deps.currentTime = []() { return time(nullptr); };
  deps.loadSavedView = []() { return gRtcSavedView; };
  deps.saveCurrentView = [](homedeck::SystemView view) { gRtcSavedView = view; };
  deps.preSleepRender = [](homedeck::SystemView view) {
    if (view == homedeck::SystemView::Almanac) {
      gAlmanacView.renderSleep();
    } else if (view == homedeck::SystemView::Calendar) {
      gCalendarView.renderSleep();
    } else if (view == homedeck::SystemView::Countdown) {
      gCountdownView.renderSleep();
    }
  };
  deps.enterDeepSleep = enterHomeDeepSleep;
  return deps;
}

}  // namespace

void appSetup() {
#ifndef UNIT_TEST
  if (esp_reset_reason() != ESP_RST_POWERON) {
    i2cBusRecovery();
  }
#endif
  auto cfg = M5.config();
  cfg.clear_display = false;
  M5.begin(cfg);
  M5.Display.setRotation(0);
#ifndef UNIT_TEST
  if (esp_reset_reason() == ESP_RST_DEEPSLEEP) {
    prepareEpdAfterDeepSleep();
  } else {
    prepareEpdAfterWakeup();
  }
#else
  prepareEpdAfterWakeup();
#endif
  initRgbLed();
  gConfigStore.begin();
  gTimeService = std::make_unique<TimeService>(makeTimeDeps());
  gBootController = std::make_unique<BootController>(makeBootDeps());
  gBootController->begin();
}

void appLoop() {
  if (gBootController) {
    gBootController->update();
  }
}

}  // namespace homedeck
