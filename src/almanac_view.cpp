#include "almanac_view.h"

#include <M5Unified.h>
#include <cstdio>
#include <ctime>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include "generated/device_font_vlw.h"
#include "render_context.h"
#include "sht40_reader.h"
#include "view_common.h"

namespace homedeck {
namespace {

constexpr int kCalendarDayTopY = 39;
constexpr int kCalendarLunarTopY = 227;
constexpr int kCalendarGanzhiTopY = 254;
constexpr int kTableLeftX = 12;
constexpr int kTableTopY = 293;
constexpr int kTableWidth = 376;
constexpr int kTableRow1BottomY = 340;
constexpr int kTableRow2BottomY = 387;
constexpr int kTableTextLeftX = 22;
constexpr int kTableTextRightX = 378;
constexpr int kTableContentLeftX = 62;
constexpr int kTableContentWidth = 316;
constexpr int kTableRow1TextY = 303;
constexpr int kTableRow2TextY = 350;
constexpr int kTableYiTextY = 397;
constexpr int kTableLineHeight = 27;
constexpr int kTableRowPaddingY = 10;
constexpr int kTableFixedRowHeight = 47;
constexpr int kMaxActionLines = 2;

constexpr std::uint16_t kGreenColor = 0x0449;
constexpr std::uint16_t kRedColor = 0xF800;


std::size_t utf8CodePointLength(unsigned char leadByte) {
  if ((leadByte & 0x80) == 0) {
    return 1;
  }
  if ((leadByte & 0xE0) == 0xC0) {
    return 2;
  }
  if ((leadByte & 0xF0) == 0xE0) {
    return 3;
  }
  if ((leadByte & 0xF8) == 0xF0) {
    return 4;
  }
  return 1;
}

int getEventPriority(const std::string& event) {
  static const std::unordered_map<std::string, int> kPriorityMap = {
      {"嫁娶", 10}, {"纳采", 11}, {"订盟", 12}, {"问名", 13}, {"纳婿", 14},
      {"归宁", 15}, {"求嗣", 16}, {"祈福", 17}, {"冠笄", 18},
      {"安葬", 20}, {"破土", 21}, {"启攒", 22}, {"入殓", 23}, {"移柩", 24},
      {"祭祀", 25}, {"普渡", 26}, {"谢土", 27},
      {"入宅", 30}, {"移徙", 31}, {"修造", 32}, {"动土", 33}, {"竖柱", 34},
      {"上梁", 35}, {"盖屋", 36}, {"安床", 37}, {"安门", 38}, {"修门", 39},
      {"出行", 40}, {"赴任", 41}, {"开市", 42}, {"立券", 43}, {"交易", 44},
      {"纳财", 45}, {"纳畜", 46}, {"置产", 47}, {"开仓", 48}, {"出货财", 49},
      {"解除", 50}, {"破屋坏垣", 51}, {"理发", 52}, {"沐浴", 53}, {"扫舍", 54},
      {"整手足甲", 55}, {"求医", 56}, {"治病", 57}, {"针灸", 58}
  };
  auto it = kPriorityMap.find(event);
  if (it != kPriorityMap.end()) {
    return it->second;
  }
  return 999;
}

std::string sortAlmanacEvents(const std::string& text) {
  std::vector<std::string> words;
  std::string current;
  for (char ch : text) {
    if (ch == ' ') {
      if (!current.empty()) {
        words.push_back(current);
        current.clear();
      }
    } else {
      current += ch;
    }
  }
  if (!current.empty()) {
    words.push_back(current);
  }

  if (words.size() <= 1) {
    return text;
  }

  std::stable_sort(words.begin(), words.end(), [](const std::string& a, const std::string& b) {
    return getEventPriority(a) < getEventPriority(b);
  });

  std::string sortedText;
  for (std::size_t i = 0; i < words.size(); ++i) {
    if (i > 0) {
      sortedText += " ";
    }
    sortedText += words[i];
  }
  return sortedText;
}

std::vector<std::string> tokenize(const std::string& text) {
  std::vector<std::string> tokens;
  std::string currentWord;
  for (std::size_t index = 0; index < text.size();) {
    std::size_t length = utf8CodePointLength(static_cast<unsigned char>(text[index]));
    if (index + length > text.size()) {
      length = 1;
    }
    std::string glyph = text.substr(index, length);
    index += length;

    if (glyph == " " || glyph == "\n") {
      if (!currentWord.empty()) {
        tokens.push_back(currentWord);
        currentWord.clear();
      }
      tokens.push_back(glyph);
    } else {
      currentWord += glyph;
    }
  }
  if (!currentWord.empty()) {
    tokens.push_back(currentWord);
  }
  return tokens;
}

void drawWrappedText(
    M5Canvas& canvas,
    const std::string& text,
    int startX,
    int startY,
    int maxWidth,
    int lineHeight,
    int maxLines) {
  int currentX = startX;
  int currentY = startY;
  int currentLine = 1;

  auto moveToNextLine = [&]() {
    if (currentLine >= maxLines) {
      return false;
    }
    ++currentLine;
    currentX = startX;
    currentY += lineHeight;
    return true;
  };

  std::vector<std::string> tokens = tokenize(text);

  for (const auto& token : tokens) {
    if (token == "\n") {
      if (!moveToNextLine()) {
        return;
      }
      continue;
    }
    if (token == " ") {
      if (currentX == startX) {
        continue;
      }
      const int spaceWidth = canvas.textWidth(" ");
      if (currentX + spaceWidth > startX + maxWidth) {
        if (!moveToNextLine()) {
          return;
        }
      } else {
        currentX += spaceWidth;
      }
      continue;
    }

    const int wordWidth = canvas.textWidth(token.c_str());
    if (currentX > startX && currentX + wordWidth > startX + maxWidth) {
      if (!moveToNextLine()) {
        return;
      }
    }

    if (wordWidth <= maxWidth) {
      canvas.drawString(token.c_str(), currentX, currentY);
      currentX += wordWidth;
    } else {
      return;
    }
  }
}

int wrappedLineCount(M5Canvas& canvas, const std::string& text, int maxWidth) {
  int lineCount = 1;
  int currentWidth = 0;

  std::vector<std::string> tokens = tokenize(text);

  for (const auto& token : tokens) {
    if (token == "\n") {
      ++lineCount;
      currentWidth = 0;
      continue;
    }
    if (token == " ") {
      if (currentWidth == 0) {
        continue;
      }
      const int spaceWidth = canvas.textWidth(" ");
      if (currentWidth + spaceWidth > maxWidth) {
        ++lineCount;
        currentWidth = 0;
      } else {
        currentWidth += spaceWidth;
      }
      continue;
    }

    const int wordWidth = canvas.textWidth(token.c_str());
    if (currentWidth > 0 && currentWidth + wordWidth > maxWidth) {
      ++lineCount;
      currentWidth = 0;
    }

    if (wordWidth <= maxWidth) {
      currentWidth += wordWidth;
    } else {
      break;
    }
  }

  return lineCount;
}

int dynamicActionRowHeight(M5Canvas& canvas, const std::string& text) {
  int lineCount = wrappedLineCount(canvas, text, kTableContentWidth);
  if (lineCount > kMaxActionLines) {
    lineCount = kMaxActionLines;
  }
  const int contentHeight = lineCount * kTableLineHeight + kTableRowPaddingY * 2;
  return contentHeight > kTableFixedRowHeight ? contentHeight : kTableFixedRowHeight;
}

std::string formatDay(int day) {
  char buffer[8] = {};
  std::snprintf(buffer, sizeof(buffer), "%d", day);
  return buffer;
}

void applyMissingAlmanac(HomeCalendarData& data) {
  data.lunarDate = "数据缺失";
  data.solarTerm = "";
  data.ganzhi = "黄历数据缺失";
  data.wuxing = "五行暂无";
  data.chongsha = "冲煞暂无";
  data.zhishen = "值神暂无";
  data.jianchu = "建除暂无";
  data.taishen = "胎神暂无";
  data.yi = "暂无";
  data.ji = "暂无";
}

void applyAlmanac(HomeCalendarData& data, const AlmanacDayData& almanac) {
  data.lunarDate = almanac.lunarDate;
  data.solarTerm = almanac.solarTerm;
  data.ganzhi = almanac.ganzhi;
  data.wuxing = almanac.wuxing;
  data.chongsha = almanac.chongsha;
  data.zhishen = almanac.zhishen;
  data.jianchu = almanac.jianchu;
  data.taishen = almanac.taishen;
  data.yi = almanac.yi.empty() ? "暂无" : almanac.yi;
  data.ji = almanac.ji.empty() ? "暂无" : almanac.ji;
}

}  // namespace

void setAlmanacCacheString(char* dest, std::size_t size, const std::string& src) {
  const std::size_t len = std::min(size - 1, src.size());
  std::memcpy(dest, src.c_str(), len);
  dest[len] = '\0';
}

#ifdef UNIT_TEST
AlmanacCache gAlmanacCache;
#else
RTC_DATA_ATTR AlmanacCache gAlmanacCache;
#endif

bool almanacCacheMatches(int year, int month, int day) {
  return gAlmanacCache.year == year && gAlmanacCache.month == month && gAlmanacCache.day == day;
}

void prepareAlmanacCacheDate(int year, int month, int day) {
  if (almanacCacheMatches(year, month, day)) {
    return;
  }
  gAlmanacCache = AlmanacCache{};
  gAlmanacCache.year = year;
  gAlmanacCache.month = month;
  gAlmanacCache.day = day;
}

void writeHomeAlmanacCache(int year, int month, int day, const AlmanacDayData& almanac) {
  prepareAlmanacCacheDate(year, month, day);
  gAlmanacCache.home.valid = true;
  setAlmanacCacheString(gAlmanacCache.home.lunarDate, sizeof(gAlmanacCache.home.lunarDate), almanac.lunarDate);
  setAlmanacCacheString(gAlmanacCache.home.solarTerm, sizeof(gAlmanacCache.home.solarTerm), almanac.solarTerm);
  setAlmanacCacheString(gAlmanacCache.home.ganzhi, sizeof(gAlmanacCache.home.ganzhi), almanac.ganzhi);
  setAlmanacCacheString(gAlmanacCache.home.wuxing, sizeof(gAlmanacCache.home.wuxing), almanac.wuxing);
  setAlmanacCacheString(gAlmanacCache.home.chongsha, sizeof(gAlmanacCache.home.chongsha), almanac.chongsha);
  setAlmanacCacheString(gAlmanacCache.home.zhishen, sizeof(gAlmanacCache.home.zhishen), almanac.zhishen);
  setAlmanacCacheString(gAlmanacCache.home.jianchu, sizeof(gAlmanacCache.home.jianchu), almanac.jianchu);
  setAlmanacCacheString(gAlmanacCache.home.taishen, sizeof(gAlmanacCache.home.taishen), almanac.taishen);
  setAlmanacCacheString(gAlmanacCache.home.yi, sizeof(gAlmanacCache.home.yi), almanac.yi.empty() ? "暂无" : almanac.yi);
  setAlmanacCacheString(gAlmanacCache.home.ji, sizeof(gAlmanacCache.home.ji), almanac.ji.empty() ? "暂无" : almanac.ji);
}

bool applyCachedHomeAlmanac(int year, int month, int day, HomeCalendarData& data) {
  if (!almanacCacheMatches(year, month, day) || !gAlmanacCache.home.valid) {
    return false;
  }
  data.lunarDate = gAlmanacCache.home.lunarDate;
  data.solarTerm = gAlmanacCache.home.solarTerm;
  data.ganzhi = gAlmanacCache.home.ganzhi;
  data.wuxing = gAlmanacCache.home.wuxing;
  data.chongsha = gAlmanacCache.home.chongsha;
  data.zhishen = gAlmanacCache.home.zhishen;
  data.jianchu = gAlmanacCache.home.jianchu;
  data.taishen = gAlmanacCache.home.taishen;
  data.yi = gAlmanacCache.home.yi;
  data.ji = gAlmanacCache.home.ji;
  return true;
}

std::string lookupLunarFestival(const std::string& lunarDate) {
  static const std::unordered_map<std::string, std::string> kFestivals = {
      {"正月初一", "春节"},
      {"正月十五", "元宵节"},
      {"五月初五", "端午节"},
      {"七月初七", "七夕"},
      {"七月十五", "中元节"},
      {"八月十五", "中秋节"},
      {"九月初九", "重阳节"},
      {"腊月初八", "腊八节"},
      {"腊月廿三", "小年"},
      {"腊月廿四", "小年"},
  };
  auto it = kFestivals.find(lunarDate);
  return (it != kFestivals.end()) ? it->second : "";
}

HomeCalendarData makeHomeCalendarData(const std::tm& localTime) {
  const int year = localTime.tm_year + 1900;
  const int month = localTime.tm_mon + 1;
  const int day = localTime.tm_mday;
  const int weekday = localTime.tm_wday;
  HomeCalendarData data{};
  data.year = formatYear(year);
  data.month = chineseMonthName(localTime.tm_mon);
  data.day = formatDay(day);
  data.weekday = weekdayName(weekday);
  data.isHoliday = weekday == 0 || weekday == 6;

  if (applyCachedHomeAlmanac(year, month, day, data)) {
    return data;
  }

  AlmanacProvider provider;
  AlmanacDayData almanac{};
  if (provider.lookup(year, month, day, &almanac)) {
    applyAlmanac(data, almanac);
    writeHomeAlmanacCache(year, month, day, almanac);
  } else {
    applyMissingAlmanac(data);
  }

  return data;
}

HomeCalendarData makeCurrentHomeCalendarData() {
  const std::time_t now = std::time(nullptr);
  std::tm buf{};
  const std::tm* local = now > 0 ? localtime_r(&now, &buf) : nullptr;
  return makeHomeCalendarData(local != nullptr ? *local : fallbackLocalTime());
}

#ifdef UNIT_TEST
void resetAlmanacCacheForTest() {
  gAlmanacCache = AlmanacCache{};
}
#endif

namespace {

}  // namespace

void AlmanacView::render(const HomeCalendarData& data) {
  M5Canvas& canvas = sprite();
  prepareScreen(canvas);

  const std::string sortedYi = sortAlmanacEvents(data.yi);
  const std::string sortedJi = sortAlmanacEvents(data.ji);
  const std::uint16_t themeColor = TFT_BLACK;

  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(themeColor, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString(data.year.c_str(), kViewInsetX, kViewHeaderTopY);

    canvas.setTextDatum(textdatum_t::top_center);
    canvas.drawString(data.month.c_str(), kViewCenterX, kViewHeaderTopY);

    canvas.setTextDatum(textdatum_t::top_right);
    canvas.drawString(data.weekday.c_str(), kViewRightX, kViewHeaderTopY);
    canvas.unloadFont();
  }

  if (canvas.loadFont(generated::kDeviceLargeDateFontVlw)) {
    canvas.setTextColor(themeColor, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::top_center);
    canvas.drawString(data.day.c_str(), kViewCenterX, kCalendarDayTopY);
    canvas.unloadFont();
  }

  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(themeColor, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::top_center);

    std::string lunarLine = data.lunarDate;
    if (!data.solarTerm.empty()) {
      lunarLine += " " + data.solarTerm;
    }
    canvas.drawString(lunarLine.c_str(), kViewCenterX, kCalendarLunarTopY);
    canvas.drawString(data.ganzhi.c_str(), kViewCenterX, kCalendarGanzhiTopY);

    const int yiRowTop = kTableRow2BottomY;
    const int yiRowHeight = dynamicActionRowHeight(canvas, sortedYi);
    const int yiRowBottomY = yiRowTop + yiRowHeight;
    const int jiRowTop = yiRowBottomY;
    const int jiTextY = jiRowTop + kTableRowPaddingY;
    const int jiRowHeight = dynamicActionRowHeight(canvas, sortedJi);
    const int tableHeight = kTableFixedRowHeight * 2 + yiRowHeight + jiRowHeight;

    canvas.drawRect(kTableLeftX, kTableTopY, kTableWidth, tableHeight, themeColor);
    canvas.drawFastHLine(kTableLeftX, kTableRow1BottomY, kTableWidth, themeColor);
    canvas.drawFastHLine(kTableLeftX, kTableRow2BottomY, kTableWidth, themeColor);
    canvas.drawFastHLine(kTableLeftX, yiRowBottomY, kTableWidth, themeColor);

    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString(data.wuxing.c_str(), kTableTextLeftX, kTableRow1TextY);

    canvas.setTextDatum(textdatum_t::top_center);
    canvas.drawString(data.chongsha.c_str(), kViewCenterX, kTableRow1TextY);

    canvas.setTextDatum(textdatum_t::top_right);
    canvas.drawString(data.zhishen.c_str(), kTableTextRightX, kTableRow1TextY);

    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString(data.jianchu.c_str(), kTableTextLeftX, kTableRow2TextY);

    canvas.setTextDatum(textdatum_t::top_right);
    canvas.drawString(data.taishen.c_str(), kTableTextRightX, kTableRow2TextY);

    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString("宜", kTableTextLeftX, kTableYiTextY);
    drawWrappedText(canvas, sortedYi, kTableContentLeftX, kTableYiTextY, kTableContentWidth, kTableLineHeight, kMaxActionLines);

    canvas.drawString("忌", kTableTextLeftX, jiTextY);
    drawWrappedText(canvas, sortedJi, kTableContentLeftX, jiTextY, kTableContentWidth, kTableLineHeight, kMaxActionLines);

    drawBottomStatusBar(canvas, {data.temperatureAvailable, data.temperatureCelsius,
                                  data.humidityAvailable, data.humidityPercent,
                                  data.bottomCenterMessage});

    canvas.unloadFont();
  }

  pushScreen(canvas);
}

void AlmanacView::render() {
  std::time_t now = time(nullptr);
  std::tm buf{};
  std::tm* local = now > 0 ? localtime_r(&now, &buf) : nullptr;
  std::tm fallback = fallbackLocalTime();
  std::tm targetTm = local != nullptr ? *local : fallback;

  targetTm.tm_mday += dayOffset_;
  if (std::mktime(&targetTm) == -1) {
    targetTm = fallback;
  }

  HomeCalendarData data = makeHomeCalendarData(targetTm);

  const EnvironmentReading reading = readSht40Environment();
  if (reading.ok) {
    data.temperatureAvailable = true;
    data.temperatureCelsius = reading.temperatureCelsius;
    data.humidityAvailable = true;
    data.humidityPercent = reading.humidityPercent;
  }
  data.bottomCenterMessage = formatCurrentTimeHHMM();

  render(data);
}

void AlmanacView::renderWithOffset(int dayOffset) {
  dayOffset_ = dayOffset;
  render();
}

void AlmanacView::renderSleep() {
  std::time_t now = time(nullptr);
  std::tm buf{};
  std::tm* local = now > 0 ? localtime_r(&now, &buf) : nullptr;
  std::tm fallback = fallbackLocalTime();
  std::tm targetTm = local != nullptr ? *local : fallback;

  targetTm.tm_mday += dayOffset_;
  if (std::mktime(&targetTm) == -1) {
    targetTm = fallback;
  }

  HomeCalendarData data = makeHomeCalendarData(targetTm);
  data.temperatureAvailable = false;
  data.humidityAvailable = false;
  data.bottomCenterMessage = "--:--";

  render(data);
}

void AlmanacView::onButtonA() {
  if (dayOffset_ > -3650) {
    dayOffset_--;
    renderWithOffset(dayOffset_);
  }
}

void AlmanacView::onButtonB() {
  if (dayOffset_ < 3650) {
    dayOffset_++;
    renderWithOffset(dayOffset_);
  }
}

void AlmanacView::reset() {
  dayOffset_ = 0;
  render();
}

}  // namespace homedeck
