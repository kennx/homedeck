#include "sht40_reader.h"

#include <Arduino.h>
#include <M5Unified.h>

#include <algorithm>
#include <cstdint>

namespace homedeck {
namespace {

constexpr std::uint8_t kSht40Address = 0x44;
constexpr std::uint8_t kMeasureHighPrecisionCommand = 0xFD;
constexpr std::uint32_t kI2cFrequency = 400000;
constexpr unsigned long kSht40MeasurementDelayMs = 10;

std::uint8_t crc8(const std::uint8_t* data, int length) {
  std::uint8_t crc = 0xFF;
  for (int index = 0; index < length; ++index) {
    crc ^= data[index];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? static_cast<std::uint8_t>((crc << 1) ^ 0x31) : static_cast<std::uint8_t>(crc << 1);
    }
  }
  return crc;
}

bool crcOk(const std::uint8_t* data) {
  return crc8(data, 2) == data[2];
}

float convertTemperature(std::uint16_t raw) {
  return -45.0f + 175.0f * static_cast<float>(raw) / 65535.0f;
}

float convertHumidity(std::uint16_t raw) {
  const float humidity = -6.0f + 125.0f * static_cast<float>(raw) / 65535.0f;
  return std::max(0.0f, std::min(100.0f, humidity));
}

std::uint16_t readRaw(const std::uint8_t msb, const std::uint8_t lsb) {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(msb) << 8) | lsb);
}

}  // namespace

EnvironmentReading readSht40Environment() {
  if (!M5.In_I2C.start(kSht40Address, false, kI2cFrequency)) {
    return {};
  }
  if (!M5.In_I2C.write(kMeasureHighPrecisionCommand) || !M5.In_I2C.stop()) {
    M5.In_I2C.stop();
    return {};
  }

  delay(kSht40MeasurementDelayMs);

  std::uint8_t buffer[6] = {};
  if (!M5.In_I2C.start(kSht40Address, true, kI2cFrequency)) {
    return {};
  }
  const bool readOk = M5.In_I2C.read(buffer, sizeof(buffer), true);
  const bool stopOk = M5.In_I2C.stop();
  if (!readOk || !stopOk) {
    return {};
  }

  if (!crcOk(buffer) || !crcOk(buffer + 3)) {
    return {};
  }

  EnvironmentReading reading{};
  reading.ok = true;
  reading.temperatureCelsius = convertTemperature(readRaw(buffer[0], buffer[1]));
  reading.humidityPercent = convertHumidity(readRaw(buffer[3], buffer[4]));
  return reading;
}

}  // namespace homedeck
