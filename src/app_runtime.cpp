#include "app_runtime.h"

#include <Arduino.h>
#include <ESP.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <time.h>

#ifndef UNIT_TEST
#include <driver/gpio.h>
#include <esp_rom_gpio.h>
#endif

#include <cstdio>
#include <cstdint>
#include <memory>

#include "boot_controller.h"
#include "config_portal.h"
#include "config_store.h"
#include "home_renderer.h"
#include "sht40_reader.h"
#include "time_service.h"
#include "timezone_catalog.h"
#include "wifi_connection.h"

namespace homedeck {
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

ConfigStore gConfigStore;
HomeRenderer gHomeRenderer;
ConfigPortal gConfigPortal;
std::unique_ptr<TimeService> gTimeService;
std::unique_ptr<BootController> gBootController;

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
  configTzTime(posixTimezone.c_str(), ntpServer.c_str());
  const unsigned long startedAt = millis();
  while (millis() - startedAt < 10000) {
    const time_t now = time(nullptr);
    if (now >= 1704067200) {
      *syncedUnix = now;
      return true;
    }
    delay(250);
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
    if (dt.date.year == utc->tm_year + 1900 &&
        dt.date.month == utc->tm_mon + 1 &&
        dt.date.date == utc->tm_mday &&
        dt.time.hours == utc->tm_hour &&
        dt.time.minutes == utc->tm_min &&
        dt.time.seconds == utc->tm_sec) {
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
  HomeCalendarData data = makeCurrentHomeCalendarData();
  const EnvironmentReading reading = readSht40Environment();
  if (reading.ok) {
    data.temperatureAvailable = true;
    data.temperatureCelsius = reading.temperatureCelsius;
    data.humidityAvailable = true;
    data.humidityPercent = reading.humidityPercent;
  }
  gHomeRenderer.render(data);
}

}  // namespace

void enterHomeDeepSleep(const HomeSleepRequest& request) {
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
  esp_deep_sleep_start();
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
  deps.renderHome = renderHomeWithEnvironment;
  deps.updateButtons = []() { M5.update(); };
  deps.areSetupButtonsPressed = []() { return M5.BtnA.isPressed() && M5.BtnB.isPressed(); };
  deps.millis = []() { return millis(); };
  deps.restart = []() { ESP.restart(); };
  deps.currentTime = []() { return time(nullptr); };
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
  M5.Display.wakeup();
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
