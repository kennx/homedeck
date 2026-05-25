#include <unity.h>

#include <Arduino.h>
#include <M5Unified.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "sht40_reader.h"

namespace {

std::uint8_t sht40Crc(std::uint8_t msb, std::uint8_t lsb) {
  std::uint8_t crc = 0xFF;
  const std::uint8_t bytes[] = {msb, lsb};
  for (const std::uint8_t byte : bytes) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? static_cast<std::uint8_t>((crc << 1) ^ 0x31) : static_cast<std::uint8_t>(crc << 1);
    }
  }
  return crc;
}

std::array<std::uint8_t, 6> makeSht40Frame(std::uint16_t tempRaw, std::uint16_t humidityRaw) {
  const auto tMsb = static_cast<std::uint8_t>(tempRaw >> 8);
  const auto tLsb = static_cast<std::uint8_t>(tempRaw & 0xFF);
  const auto hMsb = static_cast<std::uint8_t>(humidityRaw >> 8);
  const auto hLsb = static_cast<std::uint8_t>(humidityRaw & 0xFF);
  return {tMsb, tLsb, sht40Crc(tMsb, tLsb), hMsb, hLsb, sht40Crc(hMsb, hLsb)};
}

void loadFrame(std::uint16_t tempRaw, std::uint16_t humidityRaw) {
  const auto frame = makeSht40Frame(tempRaw, humidityRaw);
  std::copy(frame.begin(), frame.end(), M5.In_I2C.nextReadBuffer.begin());
}

}  // namespace

void setUp() {
  M5 = FakeM5Global{};
  fakeArduinoResetClock();
}

void tearDown() {
}

void test_sht40_reader_reads_temperature_and_humidity() {
  M5.In_I2C.enabled = true;
  loadFrame(28086, 29360);

  const auto reading = homedeck::readSht40Environment();

  TEST_ASSERT_TRUE(reading.ok);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 30.0f, reading.temperatureCelsius);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 50.0f, reading.humidityPercent);
  TEST_ASSERT_EQUAL(2, M5.In_I2C.startCalls);
  TEST_ASSERT_EQUAL(0x44, M5.In_I2C.lastAddress);
  TEST_ASSERT_TRUE(M5.In_I2C.lastReadMode);
  TEST_ASSERT_EQUAL(400000u, M5.In_I2C.lastFrequency);
  TEST_ASSERT_EQUAL(1, static_cast<int>(M5.In_I2C.writtenBytes.size()));
  TEST_ASSERT_EQUAL_HEX8(0xFD, M5.In_I2C.writtenBytes[0]);
  TEST_ASSERT_GREATER_OR_EQUAL(10u, gFakeMillis);
}

void test_sht40_reader_fails_when_i2c_is_disabled() {
  M5.In_I2C.enabled = false;

  const auto reading = homedeck::readSht40Environment();

  TEST_ASSERT_FALSE(reading.ok);
}

void test_sht40_reader_fails_on_crc_error() {
  M5.In_I2C.enabled = true;
  loadFrame(28086, 29360);
  M5.In_I2C.nextReadBuffer[2] ^= 0x01;

  const auto reading = homedeck::readSht40Environment();

  TEST_ASSERT_FALSE(reading.ok);
}

void test_sht40_reader_clamps_humidity_to_display_range() {
  M5.In_I2C.enabled = true;
  loadFrame(28086, 65535);

  const auto reading = homedeck::readSht40Environment();

  TEST_ASSERT_TRUE(reading.ok);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, reading.humidityPercent);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_sht40_reader_reads_temperature_and_humidity);
  RUN_TEST(test_sht40_reader_fails_when_i2c_is_disabled);
  RUN_TEST(test_sht40_reader_fails_on_crc_error);
  RUN_TEST(test_sht40_reader_clamps_humidity_to_display_range);
  return UNITY_END();
}
