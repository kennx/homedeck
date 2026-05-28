#include <unity.h>

#include <LittleFS.h>
#include <M5Unified.h>
#include <qrcode.h>

#include <ctime>
#include <string>

#include "../support/almanac_fixture.h"
#include "home_renderer.h"

namespace {

constexpr int kCenterX = 200;
constexpr int kLogoLeft = 52;
constexpr int kLogoTop = 86;
constexpr int kLogoWidth = 297;
constexpr int kLogoHeight = 40;
constexpr int kApTextCenterY = 165;
constexpr int kIpTextCenterY = 218;
constexpr int kQrLeft = 72;
constexpr int kQrTop = 258;
constexpr int kQrSize = 256;
constexpr std::uint32_t kHolidayColor = 0xF800;
constexpr int kEnvironmentTextBottomY = 588;
constexpr std::uint32_t kWeekdayColor = 0x0449;

homedeck::HomeCalendarData figmaCalendarData() {
  return {
      "2026 年",
      "十二月",
      "21",
      "星期五",
      false,
      "四月初六",
      "小满",
      "丙午年 癸巳月 丁酉日 鸡日",
      "五行炉中火",
      "冲猴煞北",
      "值神白虎",
      "建除成日",
      "胎神厨灶炉外正南",
      "出行 搬家 搬新房 动土 祈福 安床 祭祀 修造 拆卸 起基 出火 伐木 开光 求子",
      "结婚 交易 开业 安葬 修坟 行丧"};
}

void assertRectsInsideQrBounds() {
  TEST_ASSERT_GREATER_THAN(0, static_cast<int>(M5.Display.rects.size()));
  for (const auto& rect : M5.Display.rects) {
    TEST_ASSERT_GREATER_OR_EQUAL(kQrLeft, rect.x);
    TEST_ASSERT_GREATER_OR_EQUAL(kQrTop, rect.y);
    TEST_ASSERT_LESS_OR_EQUAL(kQrLeft + kQrSize, rect.x + rect.w);
    TEST_ASSERT_LESS_OR_EQUAL(kQrTop + kQrSize, rect.y + rect.h);
    TEST_ASSERT_EQUAL(TFT_BLACK, rect.color);
  }
}

int countPrintedGlyphsInRow(int y) {
  int count = 0;
  for (const auto& print : M5.Display.prints) {
    if (print.y == y && print.text.size() > 0 && print.text != "宜" && print.text != "忌") {
      ++count;
    }
  }
  return count;
}

}  // namespace

void test_home_calendar_data_uses_almanac_package_when_available() {
  fakeLittleFSSetFile("/almanac.bin", homedeck::test::buildSingleDayFixturePackage());

  std::tm local{};
  local.tm_year = 1900 - 1900;
  local.tm_mon = 0;
  local.tm_mday = 1;
  local.tm_wday = 1;

  const auto data = homedeck::makeHomeCalendarData(local);

  TEST_ASSERT_EQUAL_STRING("1900 年", data.year.c_str());
  TEST_ASSERT_EQUAL_STRING("一月", data.month.c_str());
  TEST_ASSERT_EQUAL_STRING("1", data.day.c_str());
  TEST_ASSERT_EQUAL_STRING("星期一", data.weekday.c_str());
  TEST_ASSERT_FALSE(data.isHoliday);
  TEST_ASSERT_EQUAL_STRING("腊月初一", data.lunarDate.c_str());
  TEST_ASSERT_EQUAL_STRING("", data.solarTerm.c_str());
  TEST_ASSERT_EQUAL_STRING("己亥年 丙子月 甲子日 鼠日", data.ganzhi.c_str());
  TEST_ASSERT_EQUAL_STRING("五行海中金", data.wuxing.c_str());
  TEST_ASSERT_EQUAL_STRING("冲马煞南", data.chongsha.c_str());
  TEST_ASSERT_EQUAL_STRING("值神青龙", data.zhishen.c_str());
  TEST_ASSERT_EQUAL_STRING("建除建日", data.jianchu.c_str());
  TEST_ASSERT_EQUAL_STRING("胎神占门碓外东南", data.taishen.c_str());
  TEST_ASSERT_EQUAL_STRING("祭祀 祈福", data.yi.c_str());
  TEST_ASSERT_EQUAL_STRING("嫁娶", data.ji.c_str());
}

void test_home_calendar_data_uses_placeholder_for_empty_almanac_actions() {
  auto day = homedeck::test::singleDayFixture();
  day.yi.clear();
  day.ji.clear();
  fakeLittleFSSetFile("/almanac.bin", homedeck::test::buildAlmanacFixturePackage(1900, 1, 1, {day}));

  std::tm local{};
  local.tm_year = 1900 - 1900;
  local.tm_mon = 0;
  local.tm_mday = 1;
  local.tm_wday = 1;

  const auto data = homedeck::makeHomeCalendarData(local);

  TEST_ASSERT_EQUAL_STRING("暂无", data.yi.c_str());
  TEST_ASSERT_EQUAL_STRING("暂无", data.ji.c_str());
}

void test_home_calendar_data_keeps_public_date_when_almanac_missing() {
  std::tm local{};
  local.tm_year = 2030 - 1900;
  local.tm_mon = 8;
  local.tm_mday = 8;
  local.tm_wday = 0;

  const auto data = homedeck::makeHomeCalendarData(local);

  TEST_ASSERT_EQUAL_STRING("2030 年", data.year.c_str());
  TEST_ASSERT_EQUAL_STRING("九月", data.month.c_str());
  TEST_ASSERT_EQUAL_STRING("8", data.day.c_str());
  TEST_ASSERT_EQUAL_STRING("星期日", data.weekday.c_str());
  TEST_ASSERT_TRUE(data.isHoliday);
  TEST_ASSERT_EQUAL_STRING("数据缺失", data.lunarDate.c_str());
  TEST_ASSERT_EQUAL_STRING("", data.solarTerm.c_str());
  TEST_ASSERT_EQUAL_STRING("黄历数据缺失", data.ganzhi.c_str());
  TEST_ASSERT_EQUAL_STRING("五行暂无", data.wuxing.c_str());
  TEST_ASSERT_EQUAL_STRING("冲煞暂无", data.chongsha.c_str());
  TEST_ASSERT_EQUAL_STRING("值神暂无", data.zhishen.c_str());
  TEST_ASSERT_EQUAL_STRING("建除暂无", data.jianchu.c_str());
  TEST_ASSERT_EQUAL_STRING("胎神暂无", data.taishen.c_str());
  TEST_ASSERT_EQUAL_STRING("暂无", data.yi.c_str());
  TEST_ASSERT_EQUAL_STRING("暂无", data.ji.c_str());
}

void setUp() {
  M5 = FakeM5Global{};
  fakeLittleFSReset();
  gLastQrCodeText.clear();
}

void tearDown() {
}

void test_home_renderer_draws_lunar_calendar_portrait() {
  homedeck::HomeRenderer renderer;

  renderer.render(figmaCalendarData());

  TEST_ASSERT_EQUAL(0, M5.Display.rotation);
  TEST_ASSERT_EQUAL(TFT_WHITE, M5.Display.fillScreenColor);
  TEST_ASSERT_EQUAL(0, static_cast<int>(M5.Display.pngDraws.size()));
  TEST_ASSERT_EQUAL(1, static_cast<int>(M5.Display.spritePushes.size()));

  // 验证年、月、周的文本及位置
  bool foundYear = false;
  bool foundMonth = false;
  bool foundWeekday = false;
  bool foundDay = false;
  bool foundLunar = false;
  bool foundGanzhi = false;

  for (const auto& print : M5.Display.prints) {
    if (print.text == "2026 年") {
      TEST_ASSERT_EQUAL(12, print.x);
      TEST_ASSERT_EQUAL(12, print.y);
      TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(print.fontKind));
      foundYear = true;
    } else if (print.text == "十二月") {
      TEST_ASSERT_EQUAL(200, print.x);
      TEST_ASSERT_EQUAL(12, print.y);
      TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(print.fontKind));
      foundMonth = true;
    } else if (print.text == "星期五") {
      TEST_ASSERT_EQUAL(388, print.x);
      TEST_ASSERT_EQUAL(12, print.y);
      TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(print.fontKind));
      foundWeekday = true;
    } else if (print.text == "21") {
      TEST_ASSERT_EQUAL(200, print.x);
      TEST_ASSERT_EQUAL(39, print.y);
      TEST_ASSERT_EQUAL(1, print.size);
      TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kDeviceLargeDate), static_cast<int>(print.fontKind));
      foundDay = true;
    } else if (print.text == "四月初六 小满") {
      TEST_ASSERT_EQUAL(200, print.x);
      TEST_ASSERT_EQUAL(227, print.y);
      foundLunar = true;
    } else if (print.text == "丙午年 癸巳月 丁酉日 鸡日") {
      TEST_ASSERT_EQUAL(200, print.x);
      TEST_ASSERT_EQUAL(254, print.y);
      foundGanzhi = true;
    }
  }

  TEST_ASSERT_TRUE(foundYear);
  TEST_ASSERT_TRUE(foundMonth);
  TEST_ASSERT_TRUE(foundWeekday);
  TEST_ASSERT_TRUE(foundDay);
  TEST_ASSERT_TRUE(foundLunar);
  TEST_ASSERT_TRUE(foundGanzhi);

  // 验证网格边框和线条的绘制
  bool foundTableBorder = false;
  int internalLineCount = 0;

  for (const auto& rect : M5.Display.rects) {
    if (rect.x == 12 && rect.y == 293 && rect.w == 376 && rect.h == 215) {
      foundTableBorder = true;
    } else if (rect.x == 12 && rect.w == 376 && rect.h == 1) {
      if (rect.y == 340 || rect.y == 387 || rect.y == 461) {
        internalLineCount++;
      }
    }
  }

  TEST_ASSERT_TRUE(foundTableBorder);
  TEST_ASSERT_EQUAL(3, internalLineCount);
}

void test_home_renderer_wraps_unspaced_chinese_text_by_character() {
  auto data = figmaCalendarData();
  data.yi = "甲 乙 丙 丁 戊 己 庚 辛 壬 癸 子 丑 寅 卯 辰 巳 午 未 申 酉 戌 亥 甲 乙 丙 丁 戊 己 庚 辛 壬 癸";
  homedeck::HomeRenderer renderer;

  renderer.render(data);

  bool drewWholeString = false;
  bool foundFirstLine = false;
  bool foundWrappedLine = false;
  for (const auto& print : M5.Display.prints) {
    if (print.text == data.yi) {
      drewWholeString = true;
    }
    if (print.text == "甲" && print.y == 397) {
      foundFirstLine = true;
    }
    if (!print.text.empty() && print.y > 397 && print.y < 488) {
      foundWrappedLine = true;
    }
  }

  TEST_ASSERT_FALSE(drewWholeString);
  TEST_ASSERT_TRUE(foundFirstLine);
  TEST_ASSERT_TRUE(foundWrappedLine);
}

void test_home_renderer_uses_red_for_all_holiday_text_and_table_lines() {
  auto data = figmaCalendarData();
  data.weekday = "星期日";
  data.isHoliday = true;
  homedeck::HomeRenderer renderer;

  renderer.render(data);

  TEST_ASSERT_GREATER_THAN(0, static_cast<int>(M5.Display.prints.size()));
  for (const auto& print : M5.Display.prints) {
    TEST_ASSERT_EQUAL_UINT32(TFT_BLACK, print.color);
  }

  TEST_ASSERT_GREATER_THAN(0, static_cast<int>(M5.Display.rects.size()));
  for (const auto& rect : M5.Display.rects) {
    TEST_ASSERT_EQUAL_UINT32(TFT_BLACK, rect.color);
  }
}

void test_home_renderer_shrinks_yi_ji_rows_when_content_is_single_line() {
  auto data = figmaCalendarData();
  data.yi = "出行";
  data.ji = "结婚";
  homedeck::HomeRenderer renderer;

  renderer.render(data);

  bool foundTableBorder = false;
  bool foundDynamicDivider = false;
  bool foundJiLabel = false;
  bool foundJiContent = false;
  for (const auto& rect : M5.Display.rects) {
    if (rect.x == 12 && rect.y == 293 && rect.w == 376 && rect.h == 188) {
      foundTableBorder = true;
    }
    if (rect.x == 12 && rect.y == 434 && rect.w == 376 && rect.h == 1) {
      foundDynamicDivider = true;
    }
  }
  for (const auto& print : M5.Display.prints) {
    if (print.text == "忌" && print.y == 444) {
      foundJiLabel = true;
    }
    if (print.text == "结婚" && print.y == 444) {
      foundJiContent = true;
    }
  }

  TEST_ASSERT_TRUE(foundTableBorder);
  TEST_ASSERT_TRUE(foundDynamicDivider);
  TEST_ASSERT_TRUE(foundJiLabel);
  TEST_ASSERT_TRUE(foundJiContent);
}

void test_home_renderer_draws_environment_readings_at_bottom_edges() {
  auto data = figmaCalendarData();
  data.temperatureAvailable = true;
  data.temperatureCelsius = 30.04f;
  data.humidityAvailable = true;
  data.humidityPercent = 49.96f;
  homedeck::HomeRenderer renderer;

  renderer.render(data);

  bool foundTemperature = false;
  bool foundHumidity = false;
  for (const auto& print : M5.Display.prints) {
    if (print.text == "30.0°C") {
      TEST_ASSERT_EQUAL(12, print.x);
      TEST_ASSERT_EQUAL(kEnvironmentTextBottomY, print.y);
      TEST_ASSERT_EQUAL(static_cast<int>(textdatum_t::bottom_left), print.datum);
      TEST_ASSERT_EQUAL_UINT32(TFT_BLACK, print.color);
      TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(print.fontKind));
      foundTemperature = true;
    }
    if (print.text == "50.0%") {
      TEST_ASSERT_EQUAL(388, print.x);
      TEST_ASSERT_EQUAL(kEnvironmentTextBottomY, print.y);
      TEST_ASSERT_EQUAL(static_cast<int>(textdatum_t::bottom_right), print.datum);
      TEST_ASSERT_EQUAL_UINT32(TFT_BLACK, print.color);
      TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(print.fontKind));
      foundHumidity = true;
    }
  }

  TEST_ASSERT_TRUE(foundTemperature);
  TEST_ASSERT_TRUE(foundHumidity);
}

void test_home_renderer_draws_environment_placeholders_when_unavailable() {
  auto data = figmaCalendarData();
  homedeck::HomeRenderer renderer;

  renderer.render(data);

  bool foundTemperature = false;
  bool foundHumidity = false;
  for (const auto& print : M5.Display.prints) {
    if (print.text == "--.-°C") {
      TEST_ASSERT_EQUAL(12, print.x);
      TEST_ASSERT_EQUAL(kEnvironmentTextBottomY, print.y);
      TEST_ASSERT_EQUAL(static_cast<int>(textdatum_t::bottom_left), print.datum);
      foundTemperature = true;
    }
    if (print.text == "--.-%") {
      TEST_ASSERT_EQUAL(388, print.x);
      TEST_ASSERT_EQUAL(kEnvironmentTextBottomY, print.y);
      TEST_ASSERT_EQUAL(static_cast<int>(textdatum_t::bottom_right), print.datum);
      foundHumidity = true;
    }
  }

  TEST_ASSERT_TRUE(foundTemperature);
  TEST_ASSERT_TRUE(foundHumidity);
}

void test_home_renderer_does_not_draw_bottom_center_message_by_default() {
  auto data = figmaCalendarData();
  homedeck::HomeRenderer renderer;

  renderer.render(data);

  for (const auto& print : M5.Display.prints) {
    TEST_ASSERT_FALSE(print.text == "DEEP SLEEP");
  }
}

void test_home_renderer_draws_bottom_center_message_when_present() {
  auto data = figmaCalendarData();
  data.bottomCenterMessage = "14:30";
  data.temperatureAvailable = true;
  data.temperatureCelsius = 30.04f;
  data.humidityAvailable = true;
  data.humidityPercent = 49.96f;
  homedeck::HomeRenderer renderer;

  renderer.render(data);

  bool foundMessage = false;
  bool foundTemperature = false;
  bool foundHumidity = false;
  for (const auto& print : M5.Display.prints) {
    if (print.text == "14:30") {
      TEST_ASSERT_EQUAL(200, print.x);
      TEST_ASSERT_EQUAL(kEnvironmentTextBottomY, print.y);
      TEST_ASSERT_EQUAL(static_cast<int>(textdatum_t::bottom_center), print.datum);
      TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(print.fontKind));
      TEST_ASSERT_EQUAL_UINT32(TFT_BLACK, print.color);
      foundMessage = true;
    }
    if (print.text == "30.0°C") {
      TEST_ASSERT_EQUAL(12, print.x);
      TEST_ASSERT_EQUAL(kEnvironmentTextBottomY, print.y);
      TEST_ASSERT_EQUAL(static_cast<int>(textdatum_t::bottom_left), print.datum);
      TEST_ASSERT_EQUAL_UINT32(TFT_BLACK, print.color);
      TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(print.fontKind));
      foundTemperature = true;
    }
    if (print.text == "50.0%") {
      TEST_ASSERT_EQUAL(388, print.x);
      TEST_ASSERT_EQUAL(kEnvironmentTextBottomY, print.y);
      TEST_ASSERT_EQUAL(static_cast<int>(textdatum_t::bottom_right), print.datum);
      TEST_ASSERT_EQUAL_UINT32(TFT_BLACK, print.color);
      TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(print.fontKind));
      foundHumidity = true;
    }
  }

  TEST_ASSERT_TRUE(foundMessage);
  TEST_ASSERT_TRUE(foundTemperature);
  TEST_ASSERT_TRUE(foundHumidity);
}

void test_home_renderer_does_not_draw_bottom_center_message_when_empty() {
  auto data = figmaCalendarData();
  data.bottomCenterMessage = "";
  homedeck::HomeRenderer renderer;

  renderer.render(data);

  for (const auto& print : M5.Display.prints) {
    TEST_ASSERT_FALSE(print.text == "14:30");
  }
}

void test_home_renderer_limits_yi_and_ji_to_two_lines() {
  auto data = figmaCalendarData();
  data.yi = "甲 乙 丙 丁 戊 己 庚 辛 壬 癸 子 丑 寅 卯 辰 巳 午 未 申 酉 戌 亥 甲 乙 丙 丁 戊 己 庚 辛 壬 癸 子 丑 寅 卯 辰 巳 午 未 申 酉 戌 亥";
  data.ji = data.yi;
  homedeck::HomeRenderer renderer;

  renderer.render(data);

  TEST_ASSERT_GREATER_THAN(0, countPrintedGlyphsInRow(397));
  TEST_ASSERT_GREATER_THAN(0, countPrintedGlyphsInRow(424));
  TEST_ASSERT_EQUAL(0, countPrintedGlyphsInRow(451));

  bool foundJiLabelAtCappedRow = false;
  for (const auto& print : M5.Display.prints) {
    if (print.text == "忌" && print.y == 471) {
      foundJiLabelAtCappedRow = true;
    }
    TEST_ASSERT_NOT_EQUAL(525, print.y);
    TEST_ASSERT_NOT_EQUAL(552, print.y);
  }
  TEST_ASSERT_TRUE(foundJiLabelAtCappedRow);
}

void test_home_renderer_draws_config_portal_layout() {
  homedeck::HomeRenderer renderer;

  renderer.renderConfigPortal("HomeDeck-ABCD", "192.168.4.1");

  TEST_ASSERT_EQUAL(0, M5.Display.rotation);
  TEST_ASSERT_EQUAL(TFT_WHITE, M5.Display.fillScreenColor);
  TEST_ASSERT_EQUAL(static_cast<int>(textdatum_t::middle_center), static_cast<int>(M5.Display.textDatum));
  TEST_ASSERT_EQUAL(1, static_cast<int>(M5.Display.spritePushes.size()));
  TEST_ASSERT_EQUAL(0, M5.Display.spritePushes[0].x);
  TEST_ASSERT_EQUAL(0, M5.Display.spritePushes[0].y);
  TEST_ASSERT_EQUAL(400, M5.Display.spritePushes[0].w);
  TEST_ASSERT_EQUAL(600, M5.Display.spritePushes[0].h);

  TEST_ASSERT_EQUAL(1, static_cast<int>(M5.Display.pngDraws.size()));
  TEST_ASSERT_EQUAL_STRING("/logo.png", M5.Display.pngDraws[0].path.c_str());
  TEST_ASSERT_EQUAL(kLogoLeft, M5.Display.pngDraws[0].x);
  TEST_ASSERT_EQUAL(kLogoTop, M5.Display.pngDraws[0].y);
  TEST_ASSERT_EQUAL(kLogoWidth, M5.Display.pngDraws[0].maxWidth);
  TEST_ASSERT_EQUAL(kLogoHeight, M5.Display.pngDraws[0].maxHeight);
  TEST_ASSERT_EQUAL(static_cast<int>(datum_t::top_left), M5.Display.pngDraws[0].datum);

  TEST_ASSERT_EQUAL(2, static_cast<int>(M5.Display.prints.size()));
  TEST_ASSERT_EQUAL_STRING("HomeDeck-ABCD", M5.Display.prints[0].text.c_str());
  TEST_ASSERT_EQUAL_STRING("192.168.4.1", M5.Display.prints[1].text.c_str());
  TEST_ASSERT_EQUAL(kCenterX, M5.Display.prints[0].x);
  TEST_ASSERT_EQUAL(kApTextCenterY, M5.Display.prints[0].y);
  TEST_ASSERT_EQUAL(kCenterX, M5.Display.prints[1].x);
  TEST_ASSERT_EQUAL(kIpTextCenterY, M5.Display.prints[1].y);
  TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kConfigPortal), static_cast<int>(M5.Display.prints[0].fontKind));
  TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kConfigPortal), static_cast<int>(M5.Display.prints[1].fontKind));

  TEST_ASSERT_EQUAL_STRING("WIFI:T:nopass;S:HomeDeck-ABCD;;", gLastQrCodeText.c_str());
  assertRectsInsideQrBounds();
  TEST_ASSERT_EQUAL(0, M5.Display.directRectCount);
  TEST_ASSERT_EQUAL(0, M5.Display.directPngDrawCount);
  TEST_ASSERT_EQUAL(1, M5.Display.waitDisplayCount);
}

void test_calendar_no_highlight_when_day_is_zero() {
  homedeck::CalendarData data;
  data.year = 2026;
  data.month = 4;
  data.day = 0;

  homedeck::HomeRenderer renderer;
  renderer.renderCalendar(data);

  // 日期网格区域 Y 范围: kCalDateStartY=108 到 kCalDateStartY + 6*(47+10)=450
  constexpr int kDateGridMinY = 108;
  constexpr int kDateGridMaxY = 450;

  bool foundHighlight = false;
  for (const auto& print : M5.Display.prints) {
    if (print.color == TFT_WHITE && print.y >= kDateGridMinY && print.y <= kDateGridMaxY) {
      foundHighlight = true;
    }
  }
  TEST_ASSERT_FALSE(foundHighlight);
}

void test_calendar_renders_past_month_correctly() {
  homedeck::CalendarData data;
  data.year = 2026;
  data.month = 4;
  data.day = 26;

  homedeck::HomeRenderer renderer;
  renderer.renderCalendar(data);

  bool foundYear = false;
  bool foundMonth = false;
  for (const auto& print : M5.Display.prints) {
    if (print.text == "2026 年") {
      foundYear = true;
    }
    if (print.text == "四月") {
      foundMonth = true;
    }
  }
  TEST_ASSERT_TRUE(foundYear);
  TEST_ASSERT_TRUE(foundMonth);
}

void test_calendar_draws_bottom_center_message_when_present() {
  homedeck::CalendarData data;
  data.year = 2026;
  data.month = 5;
  data.day = 28;
  data.bottomCenterMessage = "09:15";
  data.temperatureAvailable = true;
  data.temperatureCelsius = 25.5f;
  data.humidityAvailable = true;
  data.humidityPercent = 60.0f;

  homedeck::HomeRenderer renderer;
  renderer.renderCalendar(data);

  bool foundMessage = false;
  bool foundTemperature = false;
  bool foundHumidity = false;
  for (const auto& print : M5.Display.prints) {
    if (print.text == "09:15") {
      TEST_ASSERT_EQUAL(200, print.x);
      TEST_ASSERT_EQUAL(kEnvironmentTextBottomY, print.y);
      TEST_ASSERT_EQUAL(static_cast<int>(textdatum_t::bottom_center), print.datum);
      foundMessage = true;
    }
    if (print.text == "25.5°C") {
      foundTemperature = true;
    }
    if (print.text == "60.0%") {
      foundHumidity = true;
    }
  }

  TEST_ASSERT_TRUE(foundMessage);
  TEST_ASSERT_TRUE(foundTemperature);
  TEST_ASSERT_TRUE(foundHumidity);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_home_calendar_data_uses_almanac_package_when_available);
  RUN_TEST(test_home_calendar_data_uses_placeholder_for_empty_almanac_actions);
  RUN_TEST(test_home_calendar_data_keeps_public_date_when_almanac_missing);
  RUN_TEST(test_home_renderer_draws_lunar_calendar_portrait);
  RUN_TEST(test_home_renderer_wraps_unspaced_chinese_text_by_character);
  RUN_TEST(test_home_renderer_uses_red_for_all_holiday_text_and_table_lines);
  RUN_TEST(test_home_renderer_shrinks_yi_ji_rows_when_content_is_single_line);
  RUN_TEST(test_home_renderer_draws_environment_readings_at_bottom_edges);
  RUN_TEST(test_home_renderer_draws_environment_placeholders_when_unavailable);
  RUN_TEST(test_home_renderer_does_not_draw_bottom_center_message_by_default);
  RUN_TEST(test_home_renderer_draws_bottom_center_message_when_present);
  RUN_TEST(test_home_renderer_does_not_draw_bottom_center_message_when_empty);
  RUN_TEST(test_home_renderer_limits_yi_and_ji_to_two_lines);
  RUN_TEST(test_home_renderer_draws_config_portal_layout);
  RUN_TEST(test_calendar_no_highlight_when_day_is_zero);
  RUN_TEST(test_calendar_renders_past_month_correctly);
  RUN_TEST(test_calendar_draws_bottom_center_message_when_present);
  return UNITY_END();
}
