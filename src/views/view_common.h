#pragma once

#include <M5Unified.h>
#include <ctime>
#include <string>

namespace homedeck {

// ── 共享布局常量 ──────────────────────────────────────────────
constexpr int kViewInsetX = 12;
constexpr int kViewCenterX = 200;
constexpr int kViewRightX = 388;
constexpr int kViewHeaderTopY = 12;
constexpr int kViewBottomInset = 12;

// ── 中文日期名称 ─────────────────────────────────────────────
// monthIndex: 0-indexed (0=一月, 11=十二月)
const char* chineseMonthName(int monthIndex);

// weekdayIndex: 0=星期日, 6=星期六
const char* weekdayName(int weekdayIndex);

// ── 格式化函数 ───────────────────────────────────────────────
std::string formatYear(int year);
std::string formatTemperatureText(bool available, float celsius);
std::string formatHumidityText(bool available, float percent);

// ── 底部状态栏 ───────────────────────────────────────────────
struct StatusBarData {
  bool temperatureAvailable = false;
  float temperatureCelsius = 0.0f;
  bool humidityAvailable = false;
  float humidityPercent = 0.0f;
  std::string bottomCenterMessage;
};

void drawBottomStatusBar(M5Canvas& canvas, const StatusBarData& data);

// ── 降级时间 ─────────────────────────────────────────────────
// 设备时钟完全失效时的降级显示，固定为 1970-01-01 (周四)
std::tm fallbackLocalTime();

}  // namespace homedeck
