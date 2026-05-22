#include <unity.h>

#include "../support/fake_arduino/M5Unified.h"

FakeM5Global M5;

#include "../../../src/device_font.cpp"

void setUp() {
  M5 = FakeM5Global{};
}

void tearDown() {
}

void test_apply_default_loads_device_font_on_canvas() {
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(400, 600);

  TEST_ASSERT_TRUE(homedeck::device_font::applyDefault(canvas));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FakeFontKind::kDeviceDefault),
      static_cast<int>(M5.Display.fontKind));
}

void test_apply_default_reports_load_failure() {
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(400, 600);
  M5.Display.loadFontSucceeds = false;

  TEST_ASSERT_FALSE(homedeck::device_font::applyDefault(canvas));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FakeFontKind::kDefault),
      static_cast<int>(M5.Display.fontKind));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_apply_default_loads_device_font_on_canvas);
  RUN_TEST(test_apply_default_reports_load_failure);
  return UNITY_END();
}
