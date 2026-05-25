#include "home_renderer.h"

#include <LittleFS.h>
#include <M5Unified.h>
#include <qrcode.h>

#include <cstdio>
#include <ctime>
#include <cstdint>
#include <string>
#include <vector>

#include "almanac_provider.h"
#include "generated/device_font_vlw.h"

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
constexpr int kTableRowPaddingY = 10;
constexpr int kTableFixedRowHeight = 47;

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
  M5.Display.setRotation(0);
  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  canvas.fillSprite(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setTextDatum(textdatum_t::middle_center);
}

void pushScreen(M5Canvas& canvas) {
  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
  M5.Display.waitDisplay();
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

void drawWrappedText(M5Canvas& canvas, const std::string& text, int startX, int startY, int maxWidth, int lineHeight) {
  int currentX = startX;
  int currentY = startY;

  for (std::size_t index = 0; index < text.size();) {
    std::size_t length = utf8CodePointLength(static_cast<unsigned char>(text[index]));
    if (index + length > text.size()) {
      length = 1;
    }

    std::string glyph = text.substr(index, length);
    index += length;

    if (glyph == "\n") {
      currentX = startX;
      currentY += lineHeight;
      continue;
    }
    if (glyph == " " && currentX == startX) {
      continue;
    }

    const int glyphWidth = canvas.textWidth(glyph.c_str());
    if (currentX > startX && currentX + glyphWidth > startX + maxWidth) {
      currentX = startX;
      currentY += lineHeight;
      if (glyph == " ") {
        continue;
      }
    }

    canvas.drawString(glyph.c_str(), currentX, currentY);
    currentX += glyphWidth;
  }
}

int wrappedLineCount(M5Canvas& canvas, const std::string& text, int maxWidth) {
  int lineCount = 1;
  int currentWidth = 0;

  for (std::size_t index = 0; index < text.size();) {
    std::size_t length = utf8CodePointLength(static_cast<unsigned char>(text[index]));
    if (index + length > text.size()) {
      length = 1;
    }

    std::string glyph = text.substr(index, length);
    index += length;

    if (glyph == "\n") {
      ++lineCount;
      currentWidth = 0;
      continue;
    }
    if (glyph == " " && currentWidth == 0) {
      continue;
    }

    const int glyphWidth = canvas.textWidth(glyph.c_str());
    if (currentWidth > 0 && currentWidth + glyphWidth > maxWidth) {
      ++lineCount;
      currentWidth = 0;
      if (glyph == " ") {
        continue;
      }
    }

    currentWidth += glyphWidth;
  }

  return lineCount;
}

int dynamicActionRowHeight(M5Canvas& canvas, const std::string& text) {
  const int contentHeight = wrappedLineCount(canvas, text, kTableContentWidth) * kTableLineHeight
      + kTableRowPaddingY * 2;
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

void HomeRenderer::render() {
  const std::time_t now = std::time(nullptr);
  const std::tm* local = now > 0 ? std::localtime(&now) : nullptr;
  render(makeHomeCalendarData(local != nullptr ? *local : fallbackLocalTime()));
}

void HomeRenderer::render(const HomeCalendarData& data) {
  M5Canvas canvas(&M5.Display);
  prepareScreen(canvas);

  const std::uint16_t themeColor = data.isHoliday ? kRedColor : kGreenColor;

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
    canvas.setTextSize(2);
    canvas.drawString(data.day.c_str(), kCalendarCenterX, kCalendarDayTopY);
    canvas.setTextSize(1);
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
    const int yiRowHeight = dynamicActionRowHeight(canvas, data.yi);
    const int yiRowBottomY = yiRowTop + yiRowHeight;
    const int jiRowTop = yiRowBottomY;
    const int jiTextY = jiRowTop + kTableRowPaddingY;
    const int jiRowHeight = dynamicActionRowHeight(canvas, data.ji);
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
    drawWrappedText(canvas, data.yi, kTableContentLeftX, kTableYiTextY, kTableContentWidth, kTableLineHeight);

    canvas.drawString("忌", kTableTextLeftX, jiTextY);
    drawWrappedText(canvas, data.ji, kTableContentLeftX, jiTextY, kTableContentWidth, kTableLineHeight);

    canvas.unloadFont();
  }

  pushScreen(canvas);
}

void HomeRenderer::renderConfigPortal(const std::string& apSsid, const std::string& ipAddress) {
  M5Canvas canvas(&M5.Display);
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

}  // namespace homedeck
