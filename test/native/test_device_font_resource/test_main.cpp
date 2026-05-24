#include <unity.h>

#include "generated/device_font_vlw.h"

void test_generated_font_metadata_is_present() {
  TEST_ASSERT_GREATER_THAN_UINT32(6000U, homedeck::generated::kDeviceFontGlyphCount);
  TEST_ASSERT_GREATER_THAN_UINT32(1000000U, homedeck::generated::kDeviceFontVlwSize);
  TEST_ASSERT_EQUAL_UINT32(20U, homedeck::generated::kDeviceFontPixelSize);

  // Assertions for the new large date font
  TEST_ASSERT_GREATER_THAN_UINT32(9U, homedeck::generated::kDeviceLargeDateFontGlyphCount);
  TEST_ASSERT_GREATER_THAN_UINT32(1024U, homedeck::generated::kDeviceLargeDateFontVlwSize);
  TEST_ASSERT_EQUAL_UINT32(78U, homedeck::generated::kDeviceLargeDateFontPixelSize);
}

void test_config_portal_font_uses_misans_semibold_20px_ascii_subset() {
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(95U, homedeck::generated::kConfigPortalFontGlyphCount);
  TEST_ASSERT_GREATER_THAN_UINT32(4096U, homedeck::generated::kConfigPortalFontVlwSize);
  TEST_ASSERT_EQUAL_UINT32(20U, homedeck::generated::kConfigPortalFontPixelSize);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_generated_font_metadata_is_present);
  RUN_TEST(test_config_portal_font_uses_misans_semibold_20px_ascii_subset);
  return UNITY_END();
}
