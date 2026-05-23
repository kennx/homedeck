#include "led_service.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <cmath>

static constexpr uint8_t LED_COUNT = 2;
static constexpr std::uint8_t kM5Pm1I2cAddress = 0x6E;
static constexpr std::uint8_t kM5Pm1PowerSourceRegister = 0x04;
static constexpr std::uint8_t kM5Pm1PowerConfigRegister = 0x06;
static constexpr std::uint8_t kM5Pm1PowerSourceMask = 0x07;
static constexpr std::uint8_t kM5Pm1PowerSource5Vin = 0;
static constexpr std::uint8_t kM5Pm1LdoEnableMask = 1 << 2;

#if !defined(UNIT_TEST)
#include <Adafruit_NeoPixel.h>

static constexpr uint8_t LED_PIN   = 21;

static Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
static bool neopixelReady = false;
#else
namespace {

LedServicePixelState pixelState;

class TestNeoPixels {
 public:
  void begin() {
    pixelState.begun = true;
  }

  void setBrightness(std::uint8_t brightness) {
    pixelState.brightness = brightness;
  }

  std::uint32_t Color(std::uint8_t r, std::uint8_t g, std::uint8_t b) const {
    return (static_cast<std::uint32_t>(r) << 16) |
           (static_cast<std::uint32_t>(g) << 8) |
           static_cast<std::uint32_t>(b);
  }

  void setPixelColor(std::uint8_t index, std::uint32_t color) {
    if (index >= LED_COUNT) {
      return;
    }

    pixelState.r[index] = static_cast<std::uint8_t>((color >> 16) & 0xFF);
    pixelState.g[index] = static_cast<std::uint8_t>((color >> 8) & 0xFF);
    pixelState.b[index] = static_cast<std::uint8_t>(color & 0xFF);
  }

  void show() {
    ++pixelState.showCalls;
  }
};

TestNeoPixels pixels;
bool neopixelReady = false;

}  // namespace

const LedServicePixelState& ledServicePixelStateForTest() {
  return pixelState;
}

void resetLedServicePixelStateForTest() {
  pixelState = LedServicePixelState{};
  neopixelReady = false;
}
#endif

bool LedService::begin() {
#if !defined(UNIT_TEST)
  Serial.printf("[LedService] Board: %d, VBUS: %d mV, Bat: %d%%\n",
                (int)M5.getBoard(), M5.Power.getVBUSVoltage(), M5.Power.getBatteryLevel());

  // Ensure LDO3V3 (RGB power) is enabled via M5PM1
  if (M5.In_I2C.isEnabled()) {
    uint8_t reg06 = M5.In_I2C.readRegister8(kM5Pm1I2cAddress, kM5Pm1PowerConfigRegister, 100000);
    Serial.printf("[LedService] PMU 0x06 = 0x%02X (LDO=%d)\n", reg06, (reg06 >> 2) & 1);
    if (!(reg06 & kM5Pm1LdoEnableMask)) {
      M5.In_I2C.bitOn(kM5Pm1I2cAddress, kM5Pm1PowerConfigRegister, kM5Pm1LdoEnableMask, 100000);
      Serial.println("[LedService] LDO3V3 was off, enabled it.");
    }
  }

  // Use Adafruit_NeoPixel directly — bypasses M5.Led entirely
  pixels.begin();
  pixels.setBrightness(80);
  neopixelReady = true;
  Serial.println("[LedService] Adafruit_NeoPixel initialized on GPIO21.");
#else
  pixels.begin();
  pixels.setBrightness(80);
  neopixelReady = true;
#endif

  turnOff();
  return true;
}

bool LedService::isUsbConnected() const {
  if (M5.Power.getVBUSVoltage() > 4000) {
    return true;
  }
  if (M5.In_I2C.isEnabled()) {
    const std::uint8_t val =
        M5.In_I2C.readRegister8(kM5Pm1I2cAddress, kM5Pm1PowerSourceRegister, 100000);
    return (val & kM5Pm1PowerSourceMask) == kM5Pm1PowerSource5Vin;
  }
  return false;
}

int LedService::getBatteryLevel() const {
  return M5.Power.getBatteryLevel();
}

void LedService::setRgb(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t brightness) {
  if (neopixelReady) {
    pixels.setBrightness(brightness);
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.setPixelColor(1, pixels.Color(r, g, b));
    pixels.show();
  }
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
      setRgb(0, 255, 0, 60);
    } else {
      setRgb(255, 128, 0, 60);
    }
  } else {
    int batteryLevel = getBatteryLevel();
    if (batteryLevel >= 0 && batteryLevel < 5) {
      unsigned long ms = millis();
      float angle = (ms % 1500) * 2.0f * 3.14159265f / 1500.0f;
      std::uint8_t brightness = static_cast<std::uint8_t>((std::sin(angle) + 1.0f) * 40.0f + 10.0f);
      setRgb(255, 0, 0, brightness);
    } else {
      turnOff();
    }
  }
}
