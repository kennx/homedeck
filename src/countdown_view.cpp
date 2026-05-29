#include "countdown_view.h"

#include <M5Unified.h>
#include <cstdio>

#include "generated/device_font_vlw.h"
#include "render_context.h"

namespace homedeck {

namespace {

constexpr int kThemeColor = TFT_BLACK;
constexpr int kBgColor = TFT_WHITE;
constexpr int kHeaderTopY = 12;
constexpr int kCenterX = 200;
constexpr int kInsetX = 12;
constexpr int kRightX = 388;

const char* chineseMonthName(int monthIndex) {
  static constexpr const char* kMonths[] = {
      "一月", "二月", "三月", "四月", "五月", "六月",
      "七月", "八月", "九月", "十月", "十一月", "十二月"};
  if (monthIndex < 0 || monthIndex >= 12) {
    return kMonths[0];
  }
  return kMonths[monthIndex];
}

const char* weekdayName(int weekdayIndex) {
  static constexpr const char* kWeekdays[] = {
      "星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
  if (weekdayIndex < 0 || weekdayIndex >= 7) {
    return kWeekdays[0];
  }
  return kWeekdays[weekdayIndex];
}

std::string formatYear(int year) {
  char buffer[16] = {};
  std::snprintf(buffer, sizeof(buffer), "%d 年", year);
  return buffer;
}

int daysBetween(const std::tm& start, const std::tm& end) {
  std::tm s = start;
  std::tm e = end;
  const std::time_t startT = std::mktime(&s);
  const std::time_t endT = std::mktime(&e);
  if (startT == -1 || endT == -1) {
    return 0;
  }
  constexpr int kSecondsPerDay = 86400;
  return static_cast<int>((endT - startT) / kSecondsPerDay);
}

}  // namespace

CountdownData makeCountdownData(const std::tm& localTime) {
  CountdownData data{};
  data.currentYear = localTime.tm_year + 1900;
  data.nextYear = data.currentYear + 1;
  data.month = localTime.tm_mon + 1;
  data.weekday = localTime.tm_wday;

  std::tm startOfDay = localTime;
  startOfDay.tm_hour = 0;
  startOfDay.tm_min = 0;
  startOfDay.tm_sec = 0;

  std::tm nextYear{};
  nextYear.tm_year = localTime.tm_year + 1;
  nextYear.tm_mon = 0;
  nextYear.tm_mday = 1;
  nextYear.tm_hour = 0;
  nextYear.tm_min = 0;
  nextYear.tm_sec = 0;
  nextYear.tm_isdst = -1;

  data.daysRemaining = daysBetween(startOfDay, nextYear);
  if (data.daysRemaining < 0) {
    data.daysRemaining = 0;
  }

  return data;
}

CountdownData makeCurrentCountdownData() {
  const std::time_t now = std::time(nullptr);
  std::tm buf{};
  std::tm* local = now > 0 ? localtime_r(&now, &buf) : nullptr;
  if (local == nullptr) {
    std::tm fallback{};
    // 设备时钟完全失效时的降级显示，固定为 2026-01-01
    fallback.tm_year = 126;
    fallback.tm_mon = 0;
    fallback.tm_mday = 1;
    fallback.tm_hour = 0;
    fallback.tm_min = 0;
    fallback.tm_sec = 0;
    fallback.tm_isdst = -1;
    return makeCountdownData(fallback);
  }
  return makeCountdownData(*local);
}

void CountdownView::render() {
  CountdownData data = makeCurrentCountdownData();
  render(data);
}

void CountdownView::render(const CountdownData& data) {
  M5Canvas& canvas = sprite();
  prepareScreen(canvas);

  // 顶部状态栏：年 / 月份 / 星期几
  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(kThemeColor, kBgColor);
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString(formatYear(data.currentYear).c_str(), kInsetX, kHeaderTopY);

    canvas.setTextDatum(textdatum_t::top_center);
    canvas.drawString(chineseMonthName(data.month - 1), kCenterX, kHeaderTopY);

    canvas.setTextDatum(textdatum_t::top_right);
    canvas.drawString(weekdayName(data.weekday), kRightX, kHeaderTopY);
    canvas.unloadFont();
  }

  const int centerX = canvas.width() / 2;
  const int centerY = canvas.height() / 2;

  // 大日期字体实际渲染高度约为 pixelSize * 13/16（VLW glyph 分析得出 127px），
  // 取一半作为文本垂直半高，确保上下各 12px 间距。
  constexpr int kDaysFontHalfHeight =
      static_cast<int>(generated::kDeviceLargeDateFontPixelSize * 13 / 32);

  // 第二行：大数字天数（先绘制，避免覆盖其他元素）
  if (canvas.loadFont(generated::kDeviceLargeDateFontVlw)) {
    canvas.setTextColor(kThemeColor, kBgColor);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.drawString(std::to_string(data.daysRemaining).c_str(), centerX, centerY);
    canvas.unloadFont();
  }

  // 第一行：描述文字 "距离 2027 年还有"
  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(kThemeColor, kBgColor);
    canvas.setTextDatum(textdatum_t::bottom_center);
    char desc[32] = {};
    std::snprintf(desc, sizeof(desc), "距离 %d 年还有", data.nextYear);
    canvas.drawString(desc, centerX, centerY - kDaysFontHalfHeight - 12);
    canvas.unloadFont();
  }

  // 第三行：单位 "天"
  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(kThemeColor, kBgColor);
    canvas.setTextDatum(textdatum_t::top_center);
    canvas.drawString("天", centerX, centerY + kDaysFontHalfHeight + 12);
    canvas.unloadFont();
  }

  pushScreen(canvas);
}

void CountdownView::renderSleep() {
  CountdownData data = makeCurrentCountdownData();
  render(data);
}

}  // namespace homedeck
