#include "calendar_view.h"

#include <M5Unified.h>
#include <cstdio>
#include <ctime>
#include <string>

#include "almanac_view.h"
#include "almanac_provider.h"
#include "generated/device_font_vlw.h"
#include "render_context.h"
#include "sht40_reader.h"

namespace homedeck {
namespace {

constexpr int kCalInsetX = 12;
constexpr int kCalRightX = 388;
constexpr int kCalWidth = 376;
constexpr int kCalCenterX = 200;
constexpr int kCalHeaderTopY = 12;
constexpr int kCalHeaderHeight = 27;
constexpr int kCalWeekdayTopY = 51;
constexpr int kCalWeekdayHeight = 47;
constexpr int kCalDateStartY = 98;
constexpr int kCalDateRowHeight = 47;
constexpr int kCalDateRowGap = 0;
constexpr int kCalColCount = 7;
constexpr int kCalDateRows = 6;

const char* calendarWeekdayLabel(int index) {
  static constexpr const char* kLabels[] = {"日", "一", "二", "三", "四", "五", "六"};
  if (index < 0 || index >= 7) return "";
  return kLabels[index];
}

std::string formatCalendarYear(int year) {
  char buffer[16] = {};
  std::snprintf(buffer, sizeof(buffer), "%d 年", year);
  return buffer;
}

std::string formatCalendarMonth(int month) {
  static constexpr const char* kNames[] = {
      "一月", "二月", "三月", "四月", "五月", "六月",
      "七月", "八月", "九月", "十月", "十一月", "十二月"};
  if (month < 1 || month > 12) return "";
  return kNames[month - 1];
}

std::string formatCalendarWeekday(int wday) {
  static constexpr const char* kNames[] = {
      "星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
  if (wday < 0 || wday >= 7) return "";
  return kNames[wday];
}

int daysInMonth(int year, int month) {
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 31;
  if (month == 2) {
    const bool isLeap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    return isLeap ? 29 : 28;
  }
  return kDays[month - 1];
}

int cellLeftX(int col) {
  return kCalInsetX + col * kCalWidth / kCalColCount;
}

int cellRightX(int col) {
  return kCalInsetX + (col + 1) * kCalWidth / kCalColCount;
}

int cellCenterX(int col) {
  return (cellLeftX(col) + cellRightX(col)) / 2;
}

std::tm fallbackLocalTime() {
  std::tm local{};
  local.tm_year = 1970 - 1900;
  local.tm_mon = 0;
  local.tm_mday = 1;
  local.tm_wday = 4;
  return local;
}

void drawCalendarEnvironmentReadings(M5Canvas& canvas, const CalendarData& data) {
  constexpr int kBottomInset = 12;
  constexpr int kLeftX = 12;
  constexpr int kRightX = 388;
  constexpr int kCenterX = 200;
  const int bottomY = canvas.height() - kBottomInset;

  auto formatTemp = [](bool available, float celsius) -> std::string {
    if (!available) return "--.-°C";
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "%.1f°C", celsius);
    return buffer;
  };
  auto formatHum = [](bool available, float percent) -> std::string {
    if (!available) return "--.-%";
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "%.1f%%", percent);
    return buffer;
  };

  canvas.setTextDatum(textdatum_t::bottom_left);
  canvas.drawString(formatTemp(data.temperatureAvailable, data.temperatureCelsius).c_str(), kLeftX, bottomY);

  if (!data.bottomCenterMessage.empty()) {
    canvas.setTextDatum(textdatum_t::bottom_center);
    canvas.drawString(data.bottomCenterMessage.c_str(), kCenterX, bottomY);
  }

  canvas.setTextDatum(textdatum_t::bottom_right);
  canvas.drawString(formatHum(data.humidityAvailable, data.humidityPercent).c_str(), kRightX, bottomY);
}

}  // namespace

void CalendarView::render(const CalendarData& data) {
  M5Canvas& canvas = sprite();
  prepareScreen(canvas);
  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString(formatCalendarYear(data.year).c_str(), kCalInsetX, kCalHeaderTopY);

    canvas.setTextDatum(textdatum_t::top_center);
    canvas.drawString(formatCalendarMonth(data.month).c_str(), kCalCenterX, kCalHeaderTopY);

    canvas.setTextDatum(textdatum_t::top_right);
    canvas.drawString(formatCalendarWeekday(data.todayWeekday).c_str(), kCalRightX, kCalHeaderTopY);

    canvas.setTextDatum(textdatum_t::middle_center);
    for (int col = 0; col < kCalColCount; ++col) {
      const int cx = cellCenterX(col);
      const int cy = kCalWeekdayTopY + kCalWeekdayHeight / 2;
      canvas.drawString(calendarWeekdayLabel(col), cx, cy);
    }
    canvas.unloadFont();
  }

  std::tm firstDayTm{};
  firstDayTm.tm_year = data.year - 1900;
  firstDayTm.tm_mon = data.month - 1;
  firstDayTm.tm_mday = 1;
  std::mktime(&firstDayTm);
  const int firstWeekday = firstDayTm.tm_wday;
  const int monthDays = daysInMonth(data.year, data.month);

  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    for (int row = 0; row < kCalDateRows; ++row) {
      for (int col = 0; col < kCalColCount; ++col) {
        const int cellIndex = row * kCalColCount + col;
        const int dayNumber = cellIndex - firstWeekday + 1;

        if (dayNumber < 1 || dayNumber > monthDays) {
          continue;
        }

        const int cx = cellCenterX(col);
        const int cy = kCalDateStartY + row * (kCalDateRowHeight + kCalDateRowGap) + kCalDateRowHeight / 2;

        if (dayNumber == data.day) {
          canvas.fillCircle(cx, cy, 20, TFT_BLACK);
          canvas.setTextColor(TFT_WHITE);
        } else {
          canvas.setTextColor(TFT_BLACK, TFT_WHITE);
        }

        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.drawString(std::to_string(dayNumber).c_str(), cx, cy);
      }
    }
    canvas.unloadFont();
  }

  const int actualRows = (firstWeekday + monthDays + 6) / 7;
  const int lastRowBottomY = kCalDateStartY + actualRows * kCalDateRowHeight;
  const int sepY = lastRowBottomY + 12;
  const int sepLeftX = 12;
  const int sepRightX = canvas.width() - 12;
  canvas.drawFastHLine(sepLeftX, sepY, sepRightX - sepLeftX, TFT_BLACK);

  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);

    std::string leftText = std::to_string(data.todayMonth) + "月" + std::to_string(data.todayDay) + "日";
    if (!data.lunarDate.empty()) {
      leftText += " 农历" + data.lunarDate;
    }
    if (!data.festival.empty()) {
      leftText += " " + data.festival;
    }

    const int infoY = sepY + 10;
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString(leftText.c_str(), sepLeftX, infoY);

    if (!data.solarTerm.empty()) {
      canvas.setTextDatum(textdatum_t::top_right);
      canvas.drawString(data.solarTerm.c_str(), sepRightX, infoY);
    }

    if (data.nextSpecialMonth > 0) {
      std::string nextLine = std::to_string(data.nextSpecialMonth) + "月" +
                             std::to_string(data.nextSpecialDay) + "日";
      if (!data.nextSpecialTerm.empty()) {
        nextLine += " " + data.nextSpecialTerm;
      }
      if (!data.nextSpecialFestival.empty()) {
        if (!data.nextSpecialTerm.empty()) {
          nextLine += "、";
        }
        nextLine += data.nextSpecialFestival;
      }
      canvas.setTextDatum(textdatum_t::top_left);
      canvas.drawString(nextLine.c_str(), sepLeftX, infoY + 34);
    }

    if (data.secondSpecialMonth > 0) {
      std::string secondLine = std::to_string(data.secondSpecialMonth) + "月" +
                               std::to_string(data.secondSpecialDay) + "日";
      if (!data.secondSpecialTerm.empty()) {
        secondLine += " " + data.secondSpecialTerm;
      }
      if (!data.secondSpecialFestival.empty()) {
        if (!data.secondSpecialTerm.empty()) {
          secondLine += "、";
        }
        secondLine += data.secondSpecialFestival;
      }
      canvas.setTextDatum(textdatum_t::top_left);
      canvas.drawString(secondLine.c_str(), sepLeftX, infoY + 68);
    }
    canvas.unloadFont();
  }

  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    drawCalendarEnvironmentReadings(canvas, data);
    canvas.unloadFont();
  }

  pushScreen(canvas);
}

CalendarData makeCalendarData(const std::tm& localTime) {
  CalendarData data{};
  data.year = localTime.tm_year + 1900;
  data.month = localTime.tm_mon + 1;
  data.day = localTime.tm_mday;
  data.todayWeekday = localTime.tm_wday;
  data.todayMonth = data.month;
  data.todayDay = data.day;

  if (gAlmanacCache.year == data.year && gAlmanacCache.month == data.month &&
      gAlmanacCache.day == data.day && gAlmanacCache.calendar.valid) {
    data.lunarDate = gAlmanacCache.calendar.lunarDate;
    data.solarTerm = gAlmanacCache.calendar.solarTerm;
    data.festival = gAlmanacCache.calendar.festival;
    data.nextSpecialMonth = gAlmanacCache.calendar.nextSpecialMonth;
    data.nextSpecialDay = gAlmanacCache.calendar.nextSpecialDay;
    data.nextSpecialTerm = gAlmanacCache.calendar.nextSpecialTerm;
    data.nextSpecialFestival = gAlmanacCache.calendar.nextSpecialFestival;
    data.secondSpecialMonth = gAlmanacCache.calendar.secondSpecialMonth;
    data.secondSpecialDay = gAlmanacCache.calendar.secondSpecialDay;
    data.secondSpecialTerm = gAlmanacCache.calendar.secondSpecialTerm;
    data.secondSpecialFestival = gAlmanacCache.calendar.secondSpecialFestival;
    return data;
  }

  AlmanacProvider provider;
  std::vector<AlmanacLookupDate> dates;
  dates.reserve(36);
  dates.push_back({data.year, data.month, data.day});
  std::tm searchTm = localTime;
  for (int offset = 1; offset <= 35; ++offset) {
    searchTm.tm_mday += 1;
    searchTm.tm_hour = 12;
    std::mktime(&searchTm);
    dates.push_back({searchTm.tm_year + 1900, searchTm.tm_mon + 1, searchTm.tm_mday});
  }

  bool foundFirst = false;
  bool foundToday = false;
  const bool foundAny = provider.lookupEach(dates.data(), dates.size(), [&](const AlmanacLookupDate& date, const AlmanacDayData& almanac) {
    if (date.year == data.year && date.month == data.month && date.day == data.day) {
      foundToday = true;
      data.lunarDate = almanac.lunarDate;
      data.solarTerm = almanac.solarTerm;
      data.festival = lookupLunarFestival(almanac.lunarDate);
      writeHomeAlmanacCache(data.year, data.month, data.day, almanac);
      return false;
    }

    const std::string nextFestival = lookupLunarFestival(almanac.lunarDate);
    if (!almanac.solarTerm.empty() || !nextFestival.empty()) {
      if (!foundFirst) {
        data.nextSpecialMonth = date.month;
        data.nextSpecialDay = date.day;
        data.nextSpecialTerm = almanac.solarTerm;
        data.nextSpecialFestival = nextFestival;
        foundFirst = true;
      } else {
        data.secondSpecialMonth = date.month;
        data.secondSpecialDay = date.day;
        data.secondSpecialTerm = almanac.solarTerm;
        data.secondSpecialFestival = nextFestival;
        return true;
      }
    }
    return false;
  });

  if (!foundAny || !foundToday) {
    return data;
  }

  prepareAlmanacCacheDate(data.year, data.month, data.day);
  gAlmanacCache.calendar.valid = true;
  setAlmanacCacheString(gAlmanacCache.calendar.lunarDate, sizeof(gAlmanacCache.calendar.lunarDate), data.lunarDate);
  gAlmanacCache.calendar.solarTerm[0] = '\0';
  if (!data.solarTerm.empty()) {
    setAlmanacCacheString(gAlmanacCache.calendar.solarTerm, sizeof(gAlmanacCache.calendar.solarTerm), data.solarTerm);
  }
  setAlmanacCacheString(gAlmanacCache.calendar.festival, sizeof(gAlmanacCache.calendar.festival), data.festival);
  gAlmanacCache.calendar.nextSpecialMonth = data.nextSpecialMonth;
  gAlmanacCache.calendar.nextSpecialDay = data.nextSpecialDay;
  setAlmanacCacheString(gAlmanacCache.calendar.nextSpecialTerm, sizeof(gAlmanacCache.calendar.nextSpecialTerm), data.nextSpecialTerm);
  setAlmanacCacheString(
      gAlmanacCache.calendar.nextSpecialFestival,
      sizeof(gAlmanacCache.calendar.nextSpecialFestival),
      data.nextSpecialFestival);
  gAlmanacCache.calendar.secondSpecialMonth = data.secondSpecialMonth;
  gAlmanacCache.calendar.secondSpecialDay = data.secondSpecialDay;
  setAlmanacCacheString(
      gAlmanacCache.calendar.secondSpecialTerm,
      sizeof(gAlmanacCache.calendar.secondSpecialTerm),
      data.secondSpecialTerm);
  setAlmanacCacheString(
      gAlmanacCache.calendar.secondSpecialFestival,
      sizeof(gAlmanacCache.calendar.secondSpecialFestival),
      data.secondSpecialFestival);

  return data;
}

void applySht40ToCalendar(CalendarData& data) {
  const EnvironmentReading reading = readSht40Environment();
  if (reading.ok) {
    data.temperatureAvailable = true;
    data.temperatureCelsius = reading.temperatureCelsius;
    data.humidityAvailable = true;
    data.humidityPercent = reading.humidityPercent;
  }
}

CalendarData makeCurrentCalendarData() {
  const std::time_t now = std::time(nullptr);
  std::tm buf{};
  const std::tm* local = now > 0 ? localtime_r(&now, &buf) : nullptr;
  if (local == nullptr) {
    std::tm fallback = fallbackLocalTime();
    return makeCalendarData(fallback);
  }
  CalendarData data = makeCalendarData(*local);
  applySht40ToCalendar(data);
  return data;
}

void CalendarView::render() {
  std::time_t now = time(nullptr);
  std::tm buf{};
  std::tm* local = now > 0 ? localtime_r(&now, &buf) : nullptr;
  if (local == nullptr) {
    std::tm fallback = fallbackLocalTime();
    CalendarData data = makeCalendarData(fallback);
    applySht40ToCalendar(data);
    data.bottomCenterMessage = formatCurrentTimeHHMM();

    render(data);
    return;
  }

  int targetYear = local->tm_year + 1900;
  int targetMonth = local->tm_mon + 1 + monthOffset_;

  while (targetMonth > 12) {
    targetMonth -= 12;
    targetYear++;
  }
  while (targetMonth < 1) {
    targetMonth += 12;
    targetYear--;
  }

  CalendarData data = makeCalendarData(*local);
  data.year = targetYear;
  data.month = targetMonth;
  data.day = (monthOffset_ == 0) ? local->tm_mday : 0;

  applySht40ToCalendar(data);
  data.bottomCenterMessage = formatCurrentTimeHHMM();

  render(data);
}

void CalendarView::renderWithOffset(int monthOffset) {
  monthOffset_ = monthOffset;
  render();
}

void CalendarView::renderSleep() {
  std::time_t now = time(nullptr);
  std::tm buf{};
  std::tm* local = now > 0 ? localtime_r(&now, &buf) : nullptr;
  std::tm fallback = fallbackLocalTime();
  std::tm* pLocal = local != nullptr ? local : &fallback;

  int targetYear = pLocal->tm_year + 1900;
  int targetMonth = pLocal->tm_mon + 1 + monthOffset_;

  while (targetMonth > 12) {
    targetMonth -= 12;
    targetYear++;
  }
  while (targetMonth < 1) {
    targetMonth += 12;
    targetYear--;
  }

  CalendarData data = makeCalendarData(*pLocal);
  data.year = targetYear;
  data.month = targetMonth;
  data.day = (monthOffset_ == 0) ? pLocal->tm_mday : 0;
  data.temperatureAvailable = false;
  data.humidityAvailable = false;
  data.bottomCenterMessage = "--:--";

  render(data);
}

void CalendarView::onButtonA() {
  if (monthOffset_ > -120) {
    monthOffset_--;
    renderWithOffset(monthOffset_);
  }
}

void CalendarView::onButtonB() {
  if (monthOffset_ < 120) {
    monthOffset_++;
    renderWithOffset(monthOffset_);
  }
}

void CalendarView::reset() {
  monthOffset_ = 0;
  render();
}

}  // namespace homedeck
