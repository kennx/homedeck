#include "led_service.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <cmath>

#if !defined(UNIT_TEST)
#include <utility/led/LED_Strip_Class.hpp>
#endif

bool LedService::begin() {
#if !defined(UNIT_TEST)
  Serial.printf("[LedService] Detected Board Type: %d\n", (int)M5.getBoard());
  Serial.printf("[LedService] Initial VBUS voltage: %d mV\n", M5.Power.getVBUSVoltage());
  Serial.printf("[LedService] Initial Battery level: %d%%\n", M5.Power.getBatteryLevel());

  // 1. Force enable M5PM1 LDO3V3 (RGB LED power)
  if (M5.In_I2C.isEnabled()) {
    bool ok1 = M5.In_I2C.bitOn(0x6E, 0x06, 1 << 2, 100000);
    // Force enable GPIO3 output high (TF Card power)
    bool ok2 = M5.In_I2C.bitOff(0x6E, 0x16, 1 << 3, 100000);
    bool ok3 = M5.In_I2C.bitOn(0x6E, 0x10, 1 << 3, 100000);
    bool ok4 = M5.In_I2C.bitOff(0x6E, 0x13, 1 << 3, 100000);
    bool ok5 = M5.In_I2C.bitOn(0x6E, 0x11, 1 << 3, 100000);
    Serial.printf("[LedService] PMU write: LDO3V3_ok=%d, GPIO3_ok=%d/%d/%d/%d\n",
                  ok1, ok2, ok3, ok4, ok5);
  } else {
    Serial.println("[LedService] Warning: M5.In_I2C is not enabled!");
  }

  // 2. Fallback initialization of GPIO21 LED if M5Unified didn't set it up
  if (!M5.Led.isEnabled()) {
    Serial.println("[LedService] M5.Led is not enabled, manually initializing GPIO21 LED...");
    auto busled = std::make_shared<m5::LedBus_RMT>();
    auto buscfg = busled->getConfig();
    buscfg.pin_data = GPIO_NUM_21;
    busled->setConfig(buscfg);
    auto led_strip = std::make_shared<m5::LED_Strip_Class>();
    auto ledcfg = led_strip->getConfig();
    ledcfg.led_count = 2;
    ledcfg.byte_per_led = 3;
    led_strip->setBus(busled);
    led_strip->setConfig(ledcfg);
    M5.Led.setLedInstance(led_strip);
  } else {
    Serial.println("[LedService] M5.Led is already enabled.");
  }
#endif

  turnOff();
  return true;
}

bool LedService::isUsbConnected() const {
  if (M5.Power.getVBUSVoltage() > 4000) {
    return true;
  }
#if !defined(UNIT_TEST)
  if (M5.In_I2C.isEnabled()) {
    std::uint8_t val = M5.In_I2C.readRegister8(0x6E, 0x04, 100000);
    return (val & 0x07) == 1; // 1 = 5VIN
  }
#endif
  return false;
}

int LedService::getBatteryLevel() const {
  return M5.Power.getBatteryLevel();
}

void LedService::setRgb(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t brightness) {
  M5.Led.setBrightness(brightness);
  M5.Led.setAllColor(r, g, b);
  M5.Led.display();
}

void LedService::turnOff() {
  setRgb(0, 0, 0, 0);
}

void LedService::update() {
#if !defined(UNIT_TEST)
  static unsigned long lastLogMs = 0;
  unsigned long nowMs = millis();
  if (nowMs - lastLogMs >= 5000UL) {
    lastLogMs = nowMs;
    Serial.printf("[LedService] update: usb_conn=%d, bat=%d%%, vbus=%d mV\n",
                  isUsbConnected(), getBatteryLevel(), M5.Power.getVBUSVoltage());
  }
#endif

  if (isUsbConnected()) {
    int batteryLevel = getBatteryLevel();
    if (batteryLevel >= 100) {
      setRgb(0, 255, 0, 60); // Steady Green
    } else {
      setRgb(255, 128, 0, 60); // Steady Orange
    }
  } else {
    int batteryLevel = getBatteryLevel();
    if (batteryLevel >= 0 && batteryLevel < 5) {
      // Breathing Red LED (1.5s period) using sine wave
      unsigned long ms = millis();
      float angle = (ms % 1500) * 2.0f * 3.14159265f / 1500.0f;
      std::uint8_t brightness = static_cast<std::uint8_t>((std::sin(angle) + 1.0f) * 40.0f + 10.0f); // 10 to 90
      setRgb(255, 0, 0, brightness);
    } else {
      turnOff();
    }
  }
}
