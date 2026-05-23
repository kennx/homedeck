#include <unity.h>

#include "../../../src/generated/device_font_vlw.h"
// Native PlatformIO tests do not link src/generated objects automatically.
#include "../../../src/generated/device_font_vlw.cpp"

namespace {

std::uint32_t read_be32(const std::uint8_t* data, std::size_t offset) {
  return (static_cast<std::uint32_t>(data[offset]) << 24) |
         (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
         (static_cast<std::uint32_t>(data[offset + 2]) << 8) |
         static_cast<std::uint32_t>(data[offset + 3]);
}

}  // namespace

void test_generated_device_font_metadata_is_plausible() {
  TEST_ASSERT_GREATER_THAN_UINT32(6000,
                                  homedeck::generated::kDeviceFontGlyphCount);
  TEST_ASSERT_GREATER_THAN_UINT32(100000,
                                  homedeck::generated::kDeviceFontVlwSize);
  TEST_ASSERT_EQUAL_UINT32(14, homedeck::generated::kDeviceFontPixelSize);
  TEST_ASSERT_EQUAL_UINT32(19, homedeck::generated::kDeviceMetricFontGlyphCount);
  TEST_ASSERT_GREATER_THAN_UINT32(1000,
                                  homedeck::generated::kDeviceMetricFontVlwSize);
  TEST_ASSERT_EQUAL_UINT32(28, homedeck::generated::kDeviceMetricFontPixelSize);
  TEST_ASSERT_EQUAL_UINT32(19, homedeck::generated::kDeviceTimeFontGlyphCount);
  TEST_ASSERT_GREATER_THAN_UINT32(1000,
                                  homedeck::generated::kDeviceTimeFontVlwSize);
  TEST_ASSERT_EQUAL_UINT32(42, homedeck::generated::kDeviceTimeFontPixelSize);
}

void test_generated_device_font_header_matches_metadata() {
  TEST_ASSERT_NOT_NULL(homedeck::generated::kDeviceFontVlw);
  TEST_ASSERT_EQUAL_UINT32(homedeck::generated::kDeviceFontGlyphCount,
                           read_be32(homedeck::generated::kDeviceFontVlw, 0));
  TEST_ASSERT_EQUAL_UINT32(11, read_be32(homedeck::generated::kDeviceFontVlw, 4));
  TEST_ASSERT_GREATER_THAN_UINT32(0, read_be32(homedeck::generated::kDeviceFontVlw, 8));
  TEST_ASSERT_EQUAL_UINT32(0, read_be32(homedeck::generated::kDeviceFontVlw, 12));
  TEST_ASSERT_GREATER_THAN_UINT32(0, read_be32(homedeck::generated::kDeviceFontVlw, 16));
  TEST_ASSERT_GREATER_THAN_UINT32(0, read_be32(homedeck::generated::kDeviceFontVlw, 20));
  TEST_ASSERT_GREATER_THAN_UINT32(
      24 + homedeck::generated::kDeviceFontGlyphCount * 28,
      homedeck::generated::kDeviceFontVlwSize);
}

void test_generated_large_numeric_fonts_match_metadata() {
  TEST_ASSERT_NOT_NULL(homedeck::generated::kDeviceMetricFontVlw);
  TEST_ASSERT_NOT_NULL(homedeck::generated::kDeviceTimeFontVlw);
  TEST_ASSERT_EQUAL_UINT32(
      homedeck::generated::kDeviceMetricFontGlyphCount,
      read_be32(homedeck::generated::kDeviceMetricFontVlw, 0));
  TEST_ASSERT_EQUAL_UINT32(
      homedeck::generated::kDeviceTimeFontGlyphCount,
      read_be32(homedeck::generated::kDeviceTimeFontVlw, 0));
  TEST_ASSERT_GREATER_THAN_UINT32(
      homedeck::generated::kDeviceMetricFontVlwSize,
      homedeck::generated::kDeviceTimeFontVlwSize);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_generated_device_font_metadata_is_plausible);
  RUN_TEST(test_generated_device_font_header_matches_metadata);
  RUN_TEST(test_generated_large_numeric_fonts_match_metadata);
  return UNITY_END();
}
