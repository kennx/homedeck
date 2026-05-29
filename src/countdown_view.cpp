#include "countdown_view.h"

#include <M5Unified.h>
#include <cstdio>

#include "generated/device_font_vlw.h"
#include "render_context.h"

namespace homedeck {

namespace {

constexpr int kThemeColor = TFT_BLACK;
constexpr int kBgColor = TFT_WHITE;

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
  data.bottomCenterMessage = formatCurrentTimeHHMM();
  render(data);
}

void CountdownView::render(const CountdownData& data) {
  M5Canvas& canvas = sprite();
  prepareScreen(canvas);

  const int centerX = canvas.width() / 2;

  // 第一行：描述文字 "距离 2027 年还有"
  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(kThemeColor, kBgColor);
    canvas.setTextDatum(textdatum_t::top_center);
    char desc[32] = {};
    std::snprintf(desc, sizeof(desc), "距离 %d 年还有", data.nextYear);
    canvas.drawString(desc, centerX, 40);
    canvas.unloadFont();
  }

  // 第二行：大数字天数
  if (canvas.loadFont(generated::kDeviceLargeDateFontVlw)) {
    canvas.setTextColor(kThemeColor, kBgColor);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.drawString(std::to_string(data.daysRemaining).c_str(), centerX, canvas.height() / 2 - 10);
    canvas.unloadFont();
  }

  // 第三行：单位 "天"
  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(kThemeColor, kBgColor);
    canvas.setTextDatum(textdatum_t::bottom_center);
    canvas.drawString("天", centerX, canvas.height() - 80);
    canvas.unloadFont();
  }

  // 底部状态栏：当前时间
  if (canvas.loadFont(generated::kDeviceTimeFontVlw)) {
    canvas.setTextColor(kThemeColor, kBgColor);
    canvas.setTextDatum(textdatum_t::bottom_center);
    canvas.drawString(data.bottomCenterMessage.c_str(), centerX, canvas.height() - 20);
    canvas.unloadFont();
  }

  pushScreen(canvas);
}

void CountdownView::renderSleep() {
  CountdownData data = makeCurrentCountdownData();
  data.bottomCenterMessage = "--:--";
  render(data);
}

}  // namespace homedeck
