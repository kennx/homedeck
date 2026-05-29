#include "view_common.h"

#include <cstdio>

namespace homedeck {

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

std::string formatTemperatureText(bool available, float celsius) {
  if (!available) {
    return "--.-°C";
  }
  char buffer[16] = {};
  std::snprintf(buffer, sizeof(buffer), "%.1f°C", celsius);
  return buffer;
}

std::string formatHumidityText(bool available, float percent) {
  if (!available) {
    return "--.-%";
  }
  char buffer[16] = {};
  std::snprintf(buffer, sizeof(buffer), "%.1f%%", percent);
  return buffer;
}

void drawBottomStatusBar(M5Canvas& canvas, const StatusBarData& data) {
  const int bottomY = canvas.height() - kViewBottomInset;

  canvas.setTextDatum(textdatum_t::bottom_left);
  canvas.drawString(
      formatTemperatureText(data.temperatureAvailable, data.temperatureCelsius)
          .c_str(),
      kViewInsetX, bottomY);

  if (!data.bottomCenterMessage.empty()) {
    canvas.setTextDatum(textdatum_t::bottom_center);
    canvas.drawString(data.bottomCenterMessage.c_str(), kViewCenterX, bottomY);
  }

  canvas.setTextDatum(textdatum_t::bottom_right);
  canvas.drawString(
      formatHumidityText(data.humidityAvailable, data.humidityPercent).c_str(),
      kViewRightX, bottomY);
}

std::tm fallbackLocalTime() {
  std::tm local{};
  local.tm_year = 1970 - 1900;
  local.tm_mon = 0;
  local.tm_mday = 1;
  local.tm_wday = 4;
  return local;
}

}  // namespace homedeck
