#include <unity.h>

#include <string>

#include "../support/fake_arduino/M5Unified.h"

FakeM5Global M5;

#include "../../../src/device_font.cpp"
#include "../../../src/home_renderer.cpp"

namespace {

void resetFakes() {
  M5 = FakeM5Global{};
}

const FakePrintedText* findPrintedText(int x, int y) {
  for (const auto& entry : M5.Display.prints) {
    if (entry.x == x && entry.y == y) {
      return &entry;
    }
  }

  return nullptr;
}

const FakePrintedText* findPrintedText(const std::string& text) {
  for (const auto& entry : M5.Display.prints) {
    if (entry.text == text) {
      return &entry;
    }
  }

  return nullptr;
}

int availableWidthFor(const FakePrintedText& entry) {
  return M5.Display.width() - entry.x - 20;
}

int codePointWidthFor(FakeFontKind fontKind, unsigned char leadByte) {
  return FakeDisplay::codePointWidthFor(fontKind, leadByte);
}

int renderedWidth(const FakePrintedText& entry) {
  int width = 0;
  for (unsigned char ch : entry.text) {
    if ((ch & 0xC0) != 0x80) {
      width += codePointWidthFor(entry.fontKind, ch) * entry.size;
    }
  }
  return width;
}

int rightEdgeOf(const FakePrintedText& entry) {
  return entry.x + renderedWidth(entry);
}

int realisticLineHeightFor(const FakePrintedText& entry) {
  return FakeDisplay::lineHeightFor(entry.fontKind) * entry.size;
}

int bottomEdgeOf(const FakePrintedText& entry) {
  return entry.y + realisticLineHeightFor(entry);
}

void assertPrintWithinScreen(const FakePrintedText& entry) {
  TEST_ASSERT_LESS_OR_EQUAL_INT(M5.Display.width() - 20, rightEdgeOf(entry));
  TEST_ASSERT_LESS_OR_EQUAL_INT(M5.Display.height(), bottomEdgeOf(entry));
}

void assertPrintedFont(const std::string& text, FakeFontKind expected) {
  const FakePrintedText* entry = findPrintedText(text);
  TEST_ASSERT_NOT_NULL(entry);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(entry->fontKind));
}

}  // namespace

void test_render_shows_empty_event_message_when_event_count_is_zero() {
  resetFakes();

  homedeck::HomeViewModel model;
  model.timeText = "09:30";
  model.dateText = "2026年5月21日 星期四";
  model.lunarText = "农历 四月初五";
  model.solarTermText = "节气 小满";
  model.holidayText = "节假日 无";
  model.temperatureText = "23.7°C";
  model.humidityText = "56%";
  model.wifiConnected = true;
  model.timeSynced = true;
  model.calendarFresh = true;
  model.sensorAvailable = true;
  model.eventRows[0] = {"", "今日日程为空"};
  model.eventCount = 0;

  HomeRenderer renderer;
  renderer.render(model);

  TEST_ASSERT_NOT_NULL(findPrintedText("今日日程为空"));
}

void test_render_fits_long_holiday_and_event_text_within_screen() {
  resetFakes();

  homedeck::HomeViewModel model;
  model.timeText = "09:30";
  model.dateText = "2026年5月21日 星期四";
  model.lunarText = "农历 四月初五";
  model.solarTermText = "节气 小满";
  model.holidayText = "节假日 这是一个非常非常非常非常非常非常长的说明文本用于测试截断";
  model.temperatureText = "23.7°C";
  model.humidityText = "56%";
  model.wifiConnected = true;
  model.timeSynced = true;
  model.calendarFresh = true;
  model.sensorAvailable = true;
  model.eventRows[0] = {
      "09:00",
      "这是一个非常非常非常非常非常非常长的日程标题用于测试截断是否生效",
  };
  model.eventCount = 1;

  HomeRenderer renderer;
  renderer.render(model);

  const FakePrintedText* holiday = findPrintedText(20, 204);
  TEST_ASSERT_NOT_NULL(holiday);
  TEST_ASSERT_LESS_OR_EQUAL_INT(
      availableWidthFor(*holiday),
      renderedWidth(*holiday));

  const FakePrintedText* eventTitle = findPrintedText(112, 396);
  TEST_ASSERT_NOT_NULL(eventTitle);
  TEST_ASSERT_LESS_OR_EQUAL_INT(
      availableWidthFor(*eventTitle),
      renderedWidth(*eventTitle));
}

void test_render_truncates_holiday_text_using_device_font_metrics() {
  resetFakes();

  homedeck::HomeViewModel model;
  model.timeText = "09:30";
  model.dateText = "2026年5月21日 星期四";
  model.lunarText = "农历 四月初五";
  model.solarTermText = "节气 小满";
  model.holidayText = "一二三四五六七八九十一二三四五六七八九十一二三四五六七八九十";
  model.temperatureText = "23.7°C";
  model.humidityText = "56%";
  model.wifiConnected = true;
  model.timeSynced = true;
  model.calendarFresh = true;
  model.sensorAvailable = true;
  model.eventRows[0] = {"09:00", "开晨会"};
  model.eventCount = 1;

  HomeRenderer renderer;
  renderer.render(model);

  const FakePrintedText* holiday = findPrintedText(20, 204);
  TEST_ASSERT_NOT_NULL(holiday);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FakeFontKind::kDeviceDefault),
      static_cast<int>(holiday->fontKind));
  TEST_ASSERT_TRUE(holiday->text.size() >= 3);
  TEST_ASSERT_EQUAL_STRING("...", holiday->text.substr(holiday->text.size() - 3).c_str());
  TEST_ASSERT_LESS_OR_EQUAL_INT(availableWidthFor(*holiday), renderedWidth(*holiday));
}

void test_render_uses_device_default_font_for_body_text_fields() {
  resetFakes();

  homedeck::HomeViewModel model;
  model.timeText = "09:30";
  model.dateText = "2026年5月21日 星期四";
  model.lunarText = "农历 四月初五";
  model.solarTermText = "节气 小满";
  model.holidayText = "节假日 无";
  model.temperatureText = "23.7°C";
  model.humidityText = "56%";
  model.wifiConnected = true;
  model.timeSynced = true;
  model.calendarFresh = true;
  model.sensorAvailable = true;
  model.eventRows[0] = {"09:00", "开晨会"};
  model.eventCount = 1;

  HomeRenderer renderer;
  renderer.render(model);

  assertPrintedFont("2026年5月21日 星期四", FakeFontKind::kDeviceDefault);
  assertPrintedFont("今日日程", FakeFontKind::kDeviceDefault);
  assertPrintedFont("09:00", FakeFontKind::kDeviceDefault);
  assertPrintedFont("开晨会", FakeFontKind::kDeviceDefault);
}

void test_render_keeps_home_sections_separated_with_device_font_height() {
  resetFakes();

  homedeck::HomeViewModel model;
  model.timeText = "09:30";
  model.dateText = "2026年5月21日 星期四";
  model.lunarText = "农历 四月初五";
  model.solarTermText = "节气 小满";
  model.holidayText = "节假日 无";
  model.temperatureText = "23.7°C";
  model.humidityText = "56%";
  model.wifiConnected = true;
  model.timeSynced = true;
  model.calendarFresh = true;
  model.sensorAvailable = true;
  model.eventRows[0] = {"09:00", "开晨会"};
  model.eventCount = 1;

  HomeRenderer renderer;
  renderer.render(model);

  const FakePrintedText* time = findPrintedText("09:30");
  const FakePrintedText* date = findPrintedText("2026年5月21日 星期四");
  const FakePrintedText* lunar = findPrintedText("农历 四月初五");
  const FakePrintedText* solarTerm = findPrintedText("节气 小满");
  const FakePrintedText* holiday = findPrintedText("节假日 无");
  const FakePrintedText* temperatureLabel = findPrintedText(32, 256);
  const FakePrintedText* temperatureValue = findPrintedText(32, 288);
  const FakePrintedText* humidityLabel = findPrintedText(222, 256);
  const FakePrintedText* humidityValue = findPrintedText(222, 288);
  const FakePrintedText* eventsTitle = findPrintedText("今日日程");
  const FakePrintedText* eventTime = findPrintedText(20, 396);
  const FakePrintedText* eventTitle = findPrintedText(112, 396);
  const FakePrintedText* status = findPrintedText(20, 566);

  TEST_ASSERT_NOT_NULL(time);
  TEST_ASSERT_NOT_NULL(date);
  TEST_ASSERT_NOT_NULL(lunar);
  TEST_ASSERT_NOT_NULL(solarTerm);
  TEST_ASSERT_NOT_NULL(holiday);
  TEST_ASSERT_NOT_NULL(temperatureLabel);
  TEST_ASSERT_NOT_NULL(temperatureValue);
  TEST_ASSERT_NOT_NULL(humidityLabel);
  TEST_ASSERT_NOT_NULL(humidityValue);
  TEST_ASSERT_NOT_NULL(eventsTitle);
  TEST_ASSERT_NOT_NULL(eventTime);
  TEST_ASSERT_NOT_NULL(eventTitle);
  TEST_ASSERT_NOT_NULL(status);

  TEST_ASSERT_LESS_OR_EQUAL_INT(date->y, bottomEdgeOf(*time));
  TEST_ASSERT_LESS_OR_EQUAL_INT(lunar->y, bottomEdgeOf(*date));
  TEST_ASSERT_LESS_OR_EQUAL_INT(solarTerm->y, bottomEdgeOf(*lunar));
  TEST_ASSERT_LESS_OR_EQUAL_INT(holiday->y, bottomEdgeOf(*solarTerm));
  TEST_ASSERT_LESS_OR_EQUAL_INT(244, bottomEdgeOf(*holiday));
  TEST_ASSERT_LESS_OR_EQUAL_INT(244 + 92, bottomEdgeOf(*temperatureLabel));
  TEST_ASSERT_LESS_OR_EQUAL_INT(244 + 92, bottomEdgeOf(*temperatureValue));
  TEST_ASSERT_LESS_OR_EQUAL_INT(244 + 92, bottomEdgeOf(*humidityLabel));
  TEST_ASSERT_LESS_OR_EQUAL_INT(244 + 92, bottomEdgeOf(*humidityValue));
  TEST_ASSERT_LESS_OR_EQUAL_INT(eventTime->y, bottomEdgeOf(*eventsTitle));
  TEST_ASSERT_LESS_OR_EQUAL_INT(528, bottomEdgeOf(*eventTime));
  TEST_ASSERT_LESS_OR_EQUAL_INT(528, bottomEdgeOf(*eventTitle));
  TEST_ASSERT_LESS_OR_EQUAL_INT(600, bottomEdgeOf(*status));

  for (const auto& entry : M5.Display.prints) {
    assertPrintWithinScreen(entry);
  }
}

void test_render_keeps_wide_temperature_value_inside_metric_box() {
  resetFakes();

  homedeck::HomeViewModel model;
  model.timeText = "09:30";
  model.dateText = "2026年5月21日 星期四";
  model.lunarText = "农历 四月初五";
  model.solarTermText = "节气 小满";
  model.holidayText = "节假日 无";
  model.temperatureText = "130.0°C";
  model.humidityText = "56%";
  model.wifiConnected = true;
  model.timeSynced = true;
  model.calendarFresh = true;
  model.sensorAvailable = true;
  model.eventRows[0] = {"09:00", "开晨会"};
  model.eventCount = 1;

  HomeRenderer renderer;
  renderer.render(model);

  const FakePrintedText* temperature = findPrintedText("130.0°C");
  TEST_ASSERT_NOT_NULL(temperature);
  TEST_ASSERT_LESS_OR_EQUAL_INT(20 + 170, rightEdgeOf(*temperature));
}

void test_render_draws_large_values_with_native_size_fonts() {
  resetFakes();

  homedeck::HomeViewModel model;
  model.timeText = "09:30";
  model.dateText = "2026年5月21日 星期四";
  model.lunarText = "农历 四月初五";
  model.solarTermText = "节气 小满";
  model.holidayText = "节假日 无";
  model.temperatureText = "23.7°C";
  model.humidityText = "56%";
  model.wifiConnected = true;
  model.timeSynced = true;
  model.calendarFresh = true;
  model.sensorAvailable = true;
  model.eventRows[0] = {"09:00", "开晨会"};
  model.eventCount = 1;

  HomeRenderer renderer;
  renderer.render(model);

  const FakePrintedText* time = findPrintedText("09:30");
  const FakePrintedText* temperature = findPrintedText("23.7°C");
  const FakePrintedText* humidity = findPrintedText("56%");
  TEST_ASSERT_NOT_NULL(time);
  TEST_ASSERT_NOT_NULL(temperature);
  TEST_ASSERT_NOT_NULL(humidity);
  TEST_ASSERT_EQUAL_INT(1, time->size);
  TEST_ASSERT_EQUAL_INT(1, temperature->size);
  TEST_ASSERT_EQUAL_INT(1, humidity->size);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FakeFontKind::kDeviceTime),
      static_cast<int>(time->fontKind));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FakeFontKind::kDeviceMetric),
      static_cast<int>(temperature->fontKind));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(FakeFontKind::kDeviceMetric),
      static_cast<int>(humidity->fontKind));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_render_shows_empty_event_message_when_event_count_is_zero);
  RUN_TEST(test_render_fits_long_holiday_and_event_text_within_screen);
  RUN_TEST(test_render_truncates_holiday_text_using_device_font_metrics);
  RUN_TEST(test_render_uses_device_default_font_for_body_text_fields);
  RUN_TEST(test_render_keeps_home_sections_separated_with_device_font_height);
  RUN_TEST(test_render_keeps_wide_temperature_value_inside_metric_box);
  RUN_TEST(test_render_draws_large_values_with_native_size_fonts);
  return UNITY_END();
}
