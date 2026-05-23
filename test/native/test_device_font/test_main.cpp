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
  canvas.setCursor(0, 0);
  canvas.print("A");
  TEST_ASSERT_FALSE(M5.Display.prints.empty());
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FakeFontKind::kDeviceDefault),
      static_cast<int>(M5.Display.prints.back().fontKind));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FakeFontKind::kDeviceDefault),
      static_cast<int>(M5.Display.fontKind));
}

void test_apply_default_reports_load_failure() {
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(400, 600);

  TEST_ASSERT_TRUE(homedeck::device_font::applyDefault(canvas));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FakeFontKind::kDeviceDefault),
      static_cast<int>(M5.Display.fontKind));

  M5.Display.loadFontSucceeds = false;

  TEST_ASSERT_FALSE(homedeck::device_font::applyDefault(canvas));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FakeFontKind::kDefault),
      static_cast<int>(M5.Display.fontKind));
  canvas.setCursor(0, 0);
  canvas.print("B");
  TEST_ASSERT_FALSE(M5.Display.prints.empty());
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FakeFontKind::kDefault),
      static_cast<int>(M5.Display.prints.back().fontKind));
}

void test_apply_loads_role_specific_device_fonts() {
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(400, 600);

  TEST_ASSERT_TRUE(homedeck::device_font::apply(
      canvas,
      homedeck::device_font::Role::kMetricValue));
  canvas.setCursor(0, 0);
  canvas.print("23.7°C");
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FakeFontKind::kDeviceMetric),
      static_cast<int>(M5.Display.prints.back().fontKind));

  TEST_ASSERT_TRUE(homedeck::device_font::apply(canvas, homedeck::device_font::Role::kTime));
  canvas.setCursor(0, 40);
  canvas.print("09:30");
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FakeFontKind::kDeviceTime),
      static_cast<int>(M5.Display.prints.back().fontKind));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_apply_default_loads_device_font_on_canvas);
  RUN_TEST(test_apply_default_reports_load_failure);
  RUN_TEST(test_apply_loads_role_specific_device_fonts);
  return UNITY_END();
}
