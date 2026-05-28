#include "home_renderer.h"

#include <LittleFS.h>
#include <M5Unified.h>
#include <qrcode.h>

#include <cstdio>
#include <ctime>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include "almanac_provider.h"
#include "generated/device_font_vlw.h"
#include "sht40_reader.h"

namespace homedeck {
namespace {

constexpr int kLogoTopY = 86;
constexpr int kLogoWidth = 297;
constexpr int kLogoHeight = 40;
constexpr int kTextFrameHeight = 27;
constexpr int kApTextTopY = kLogoTopY + kLogoHeight + 26;
constexpr int kApTextCenterY = kApTextTopY + kTextFrameHeight / 2;
constexpr int kIpTextTopY = kApTextTopY + kTextFrameHeight + 26;
constexpr int kIpTextCenterY = kIpTextTopY + kTextFrameHeight / 2;
constexpr int kQrLeftX = 72;
constexpr int kQrTopY = kIpTextTopY + kTextFrameHeight + 26;
constexpr int kQrSize = 256;
constexpr int kCalendarInsetX = 12;
constexpr int kCalendarRightX = 388;
constexpr int kCalendarCenterX = 200;
constexpr int kCalendarHeaderTopY = 12;
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
constexpr int kEnvironmentTextBottomInset = 12;
constexpr int kEnvironmentTextLeftX = 12;
constexpr int kEnvironmentTextRightX = 388;
constexpr int kTableRowPaddingY = 10;
constexpr int kTableFixedRowHeight = 47;
constexpr int kMaxActionLines = 2;

// 供配置界面（Config Portal）渲染 Logo 使用的方法保留
int centerX() {
  return M5.Display.width() / 2;
}

int logoLeftX() {
  return (M5.Display.width() - kLogoWidth + 1) / 2;
}

void drawLogo(M5Canvas& canvas, int top) {
  if (!LittleFS.begin()) {
    return;
  }
  canvas.drawPngFile(
      LittleFS,
      "/logo.png",
      logoLeftX(),
      top,
      kLogoWidth,
      kLogoHeight,
      0,
      0,
      1.0f,
      1.0f,
      datum_t::top_left);
  LittleFS.end();
}

void prepareScreen(M5Canvas& canvas) {
  canvas.fillSprite(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setTextDatum(textdatum_t::middle_center);
}

void pushScreen(M5Canvas& canvas) {
  canvas.pushSprite(0, 0);
  M5.Display.waitDisplay();
}

M5Canvas& sprite() {
  static M5Canvas canvas(&M5.Display);
  static bool ready = false;
  if (!ready) {
    canvas.setColorDepth(16);
    canvas.createSprite(M5.Display.width(), M5.Display.height());
    ready = true;
  }
  return canvas;
}

void loadConfigPortalFont(M5Canvas& canvas) {
  if (!canvas.loadFont(generated::kConfigPortalFontVlw)) {
    canvas.setFont(nullptr);
  }
  canvas.setTextSize(1);
}

void drawQrCode(M5Canvas& canvas, const std::string& text, int left, int top, int size) {
  QRCode qrcode;
  std::vector<std::uint8_t> qrcodeBuffer(qrcode_getBufferSize(3));
  qrcode_initText(&qrcode, qrcodeBuffer.data(), 3, ECC_LOW, text.c_str());

  for (int y = 0; y < qrcode.size; ++y) {
    for (int x = 0; x < qrcode.size; ++x) {
      if (!qrcode_getModule(&qrcode, x, y)) {
        continue;
      }

      const int moduleLeft = left + x * size / qrcode.size;
      const int moduleTop = top + y * size / qrcode.size;
      const int moduleRight = left + (x + 1) * size / qrcode.size;
      const int moduleBottom = top + (y + 1) * size / qrcode.size;
      canvas.fillRect(
          moduleLeft,
          moduleTop,
          moduleRight - moduleLeft,
          moduleBottom - moduleTop,
          TFT_BLACK);
    }
  }
}

// 颜色映射 (#008A4C => RGB565: 0x0449, #F40000 => RGB565: 0xF800)
constexpr std::uint16_t kGreenColor = 0x0449;
constexpr std::uint16_t kRedColor = 0xF800;

std::string formatTemperatureText(const HomeCalendarData& data) {
  if (!data.temperatureAvailable) {
    return "--.-°C";
  }
  char buffer[16] = {};
  std::snprintf(buffer, sizeof(buffer), "%.1f°C", data.temperatureCelsius);
  return buffer;
}

std::string formatHumidityText(const HomeCalendarData& data) {
  if (!data.humidityAvailable) {
    return "--.-%";
  }
  char buffer[16] = {};
  std::snprintf(buffer, sizeof(buffer), "%.1f%%", data.humidityPercent);
  return buffer;
}

void drawEnvironmentReadings(M5Canvas& canvas, const HomeCalendarData& data) {
  const int bottomY = canvas.height() - kEnvironmentTextBottomInset;
  canvas.setTextDatum(textdatum_t::bottom_left);
  const std::string temperature = formatTemperatureText(data);
  canvas.drawString(temperature.c_str(), kEnvironmentTextLeftX, bottomY);

  if (!data.bottomCenterMessage.empty()) {
    canvas.setTextDatum(textdatum_t::bottom_center);
    canvas.drawString(data.bottomCenterMessage.c_str(), kCalendarCenterX, bottomY);
  }

  canvas.setTextDatum(textdatum_t::bottom_right);
  const std::string humidity = formatHumidityText(data);
  canvas.drawString(humidity.c_str(), kEnvironmentTextRightX, bottomY);
}

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
      // 1. 婚嫁、求嗣类
      {"嫁娶", 10}, {"纳采", 11}, {"订盟", 12}, {"问名", 13}, {"纳婿", 14},
      {"归宁", 15}, {"求嗣", 16}, {"祈福", 17}, {"冠笄", 18},
      // 2. 丧葬、祭祀类
      {"安葬", 20}, {"破土", 21}, {"启攒", 22}, {"入殓", 23}, {"移柩", 24},
      {"祭祀", 25}, {"普渡", 26}, {"谢土", 27},
      // 3. 建乔、动土、入宅类
      {"入宅", 30}, {"移徙", 31}, {"修造", 32}, {"动土", 33}, {"竖柱", 34},
      {"上梁", 35}, {"盖屋", 36}, {"安床", 37}, {"安门", 38}, {"修门", 39},
      // 4. 出行、开张、交易、纳财类
      {"出行", 40}, {"赴任", 41}, {"开市", 42}, {"立券", 43}, {"交易", 44},
      {"纳财", 45}, {"纳畜", 46}, {"置产", 47}, {"开仓", 48}, {"出货财", 49},
      // 5. 日常生活、解除、求医类
      {"解除", 50}, {"破屋坏垣", 51}, {"理发", 52}, {"沐浴", 53}, {"扫舍", 54},
      {"整手足甲", 55}, {"求医", 56}, {"治病", 57}, {"针灸", 58}
  };
  auto it = kPriorityMap.find(event);
  if (it != kPriorityMap.end()) {
    return it->second;
  }
  return 999; // 未识别事件默认极低优先级
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
        continue; // 行首忽略空格
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

    // 这是一个词组（Word）
    const int wordWidth = canvas.textWidth(token.c_str());
    if (currentX > startX && currentX + wordWidth > startX + maxWidth) {
      // 当前行放不下，尝试换行
      if (!moveToNextLine()) {
        return; // 换行失败，直接丢弃该词组及后续内容以防止截断
      }
    }

    // 检查在当前行是否能放下该词组
    if (wordWidth <= maxWidth) {
      canvas.drawString(token.c_str(), currentX, currentY);
      currentX += wordWidth;
    } else {
      // 词组甚至超出了单行最大宽度，选择停止排版
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
        continue; // 行首忽略空格
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

    // 这是一个词组（Word）
    const int wordWidth = canvas.textWidth(token.c_str());
    if (currentWidth > 0 && currentWidth + wordWidth > maxWidth) {
      // 当前行放不下，换行
      ++lineCount;
      currentWidth = 0;
    }

    if (wordWidth <= maxWidth) {
      currentWidth += wordWidth;
    } else {
      // 词组超出单行最大宽度，在实际绘制中会被丢弃停止，此处也直接 break 保持绝对一致
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

const char* chineseMonthName(int monthIndex) {
  static constexpr const char* kMonths[] = {
      "一月",
      "二月",
      "三月",
      "四月",
      "五月",
      "六月",
      "七月",
      "八月",
      "九月",
      "十月",
      "十一月",
      "十二月"};
  if (monthIndex < 0 || monthIndex >= 12) {
    return kMonths[0];
  }
  return kMonths[monthIndex];
}

const char* weekdayName(int weekdayIndex) {
  static constexpr const char* kWeekdays[] = {
      "星期日",
      "星期一",
      "星期二",
      "星期三",
      "星期四",
      "星期五",
      "星期六"};
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

std::string formatDay(int day) {
  char buffer[8] = {};
  std::snprintf(buffer, sizeof(buffer), "%d", day);
  return buffer;
}

std::tm fallbackLocalTime() {
  std::tm local{};
  local.tm_year = 1970 - 1900;
  local.tm_mon = 0;
  local.tm_mday = 1;
  local.tm_wday = 4;
  return local;
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

HomeCalendarData makeHomeCalendarData(const std::tm& localTime) {
  const int weekday = localTime.tm_wday;
  HomeCalendarData data{};
  data.year = formatYear(localTime.tm_year + 1900);
  data.month = chineseMonthName(localTime.tm_mon);
  data.day = formatDay(localTime.tm_mday);
  data.weekday = weekdayName(weekday);
  data.isHoliday = weekday == 0 || weekday == 6;

  AlmanacProvider provider;
  AlmanacDayData almanac{};
  if (provider.lookup(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday, &almanac)) {
    applyAlmanac(data, almanac);
  } else {
    applyMissingAlmanac(data);
  }

  return data;
}

HomeCalendarData makeCurrentHomeCalendarData() {
  const std::time_t now = std::time(nullptr);
  const std::tm* local = now > 0 ? std::localtime(&now) : nullptr;
  return makeHomeCalendarData(local != nullptr ? *local : fallbackLocalTime());
}

CalendarData makeCalendarData(const std::tm& localTime) {
  CalendarData data{};
  data.year = localTime.tm_year + 1900;
  data.month = localTime.tm_mon + 1;
  data.day = localTime.tm_mday;
  data.todayWeekday = localTime.tm_wday;
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
  const std::tm* local = now > 0 ? std::localtime(&now) : nullptr;
  if (local == nullptr) {
    std::tm fallback = fallbackLocalTime();
    return makeCalendarData(fallback);
  }
  CalendarData data = makeCalendarData(*local);
  applySht40ToCalendar(data);
  return data;
}

void HomeRenderer::render() {
  render(makeCurrentHomeCalendarData());
}

void HomeRenderer::render(const HomeCalendarData& data) {
  M5Canvas& canvas = sprite();
  prepareScreen(canvas);

  const std::string sortedYi = sortAlmanacEvents(data.yi);
  const std::string sortedJi = sortAlmanacEvents(data.ji);

  const std::uint16_t themeColor = TFT_BLACK;

  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(themeColor, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString(data.year.c_str(), kCalendarInsetX, kCalendarHeaderTopY);

    canvas.setTextDatum(textdatum_t::top_center);
    canvas.drawString(data.month.c_str(), kCalendarCenterX, kCalendarHeaderTopY);

    canvas.setTextDatum(textdatum_t::top_right);
    canvas.drawString(data.weekday.c_str(), kCalendarRightX, kCalendarHeaderTopY);
    canvas.unloadFont();
  }

  if (canvas.loadFont(generated::kDeviceLargeDateFontVlw)) {
    canvas.setTextColor(themeColor, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::top_center);
    canvas.drawString(data.day.c_str(), kCalendarCenterX, kCalendarDayTopY);
    canvas.unloadFont();
  }

  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(themeColor, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::top_center);

    std::string lunarLine = data.lunarDate;
    if (!data.solarTerm.empty()) {
      lunarLine += " " + data.solarTerm;
    }
    canvas.drawString(lunarLine.c_str(), kCalendarCenterX, kCalendarLunarTopY);
    canvas.drawString(data.ganzhi.c_str(), kCalendarCenterX, kCalendarGanzhiTopY);

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
    canvas.drawString(data.chongsha.c_str(), kCalendarCenterX, kTableRow1TextY);

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

    drawEnvironmentReadings(canvas, data);

    canvas.unloadFont();
  }

  pushScreen(canvas);
}

void HomeRenderer::renderConfigPortal(const std::string& apSsid, const std::string& ipAddress) {
  M5Canvas& canvas = sprite();
  prepareScreen(canvas);

  drawLogo(canvas, kLogoTopY);

  loadConfigPortalFont(canvas);
  canvas.drawString(apSsid.c_str(), centerX(), kApTextCenterY);
  canvas.drawString(ipAddress.c_str(), centerX(), kIpTextCenterY);
  canvas.unloadFont();

  const std::string qrText = std::string("WIFI:T:nopass;S:") + apSsid + ";;";
  drawQrCode(canvas, qrText, kQrLeftX, kQrTopY, kQrSize);
  pushScreen(canvas);
}

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

}  // namespace

void HomeRenderer::renderCalendar(const CalendarData& data) {
  M5Canvas& canvas = sprite();
  prepareScreen(canvas);

  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    // 标题行：年 |  | 月
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString(formatCalendarYear(data.year).c_str(), kCalInsetX, kCalHeaderTopY);

    canvas.setTextDatum(textdatum_t::top_right);
    canvas.drawString(formatCalendarMonth(data.month).c_str(), kCalRightX, kCalHeaderTopY);

    // 星期行
    canvas.setTextDatum(textdatum_t::middle_center);
    for (int col = 0; col < kCalColCount; ++col) {
      const int cx = cellCenterX(col);
      const int cy = kCalWeekdayTopY + kCalWeekdayHeight / 2;
      canvas.drawString(calendarWeekdayLabel(col), cx, cy);
    }
    canvas.unloadFont();
  }

  // 日期网格
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
          // 当天高亮：正圆形黑底白字
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

  // 底部温湿度
  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    HomeCalendarData envData{};
    envData.temperatureAvailable = data.temperatureAvailable;
    envData.temperatureCelsius = data.temperatureCelsius;
    envData.humidityAvailable = data.humidityAvailable;
    envData.humidityPercent = data.humidityPercent;
    envData.bottomCenterMessage = data.bottomCenterMessage;
    drawEnvironmentReadings(canvas, envData);
    canvas.unloadFont();
  }

  pushScreen(canvas);
}

}  // namespace homedeck
