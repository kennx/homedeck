#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <string>
#include <vector>

#include "generated/device_font_vlw.h"

constexpr std::uint32_t TFT_BLACK = 0x00000000u;
constexpr std::uint32_t TFT_WHITE = 0x00FFFFFFu;

enum class epd_mode_t {
  epd_quality = 0,
  epd_text = 1,
  epd_fast = 2,
  epd_fastest = 3,
};

namespace fonts {
inline const int efontCN_14 = 0;
}  // namespace fonts

enum class FakeFontKind {
  kDefault = 0,
  kChinese = 1,
  kDeviceDefault = 2,
  kDeviceMetric = 3,
  kConfigPortal = 4,
  kDeviceLargeDate = 5,
};

namespace m5 {

struct rtc_time_t {
  std::int8_t hours = -1;
  std::int8_t minutes = -1;
  std::int8_t seconds = -1;
};

struct rtc_date_t {
  std::int16_t year = 2000;
  std::int8_t month = 1;
  std::int8_t date = 1;
  std::int8_t weekDay = 0;
};

struct rtc_datetime_t {
  rtc_date_t date;
  rtc_time_t time;
};

}  // namespace m5

struct FakeRtc {
  bool enabled = false;
  bool getDateTimeOk = false;
  bool setSystemTimeFromRtcCalled = false;
  bool setDateTimeCalled = false;
  bool corruptTimezoneOnRestore = false;
  int readBackSecondsOffset = 0;
  bool voltLow = false;
  m5::rtc_datetime_t dateTime{};
  std::tm lastSetTm{};

  bool isEnabled() const {
    return enabled;
  }

  bool getDateTime(m5::rtc_datetime_t* out) const {
    if (!getDateTimeOk || out == nullptr) {
      return false;
    }
    *out = dateTime;
    if (readBackSecondsOffset != 0) {
      std::tm shifted{};
      shifted.tm_year = dateTime.date.year - 1900;
      shifted.tm_mon = dateTime.date.month - 1;
      shifted.tm_mday = dateTime.date.date;
      shifted.tm_hour = dateTime.time.hours;
      shifted.tm_min = dateTime.time.minutes;
      shifted.tm_sec = dateTime.time.seconds + readBackSecondsOffset;
      shifted.tm_isdst = 0;

      char* oldTimezone = std::getenv("TZ");
      const std::string savedTimezone = oldTimezone != nullptr ? oldTimezone : "";
      setenv("TZ", "UTC0", 1);
      tzset();
      const std::time_t shiftedUnix = std::mktime(&shifted);
      const std::tm* normalized = std::gmtime(&shiftedUnix);
      if (oldTimezone != nullptr) {
        setenv("TZ", savedTimezone.c_str(), 1);
      } else {
        unsetenv("TZ");
      }
      tzset();

      if (normalized != nullptr) {
        out->date.year = static_cast<std::int16_t>(normalized->tm_year + 1900);
        out->date.month = static_cast<std::int8_t>(normalized->tm_mon + 1);
        out->date.date = static_cast<std::int8_t>(normalized->tm_mday);
        out->date.weekDay = static_cast<std::int8_t>(normalized->tm_wday);
        out->time.hours = static_cast<std::int8_t>(normalized->tm_hour);
        out->time.minutes = static_cast<std::int8_t>(normalized->tm_min);
        out->time.seconds = static_cast<std::int8_t>(normalized->tm_sec);
      }
    }
    return true;
  }

  void setSystemTimeFromRtc() {
    setSystemTimeFromRtcCalled = true;
    if (corruptTimezoneOnRestore) {
      setenv("TZ", "GMT0", 1);
      tzset();
    }
  }

  void setDateTime(const std::tm* datetime) {
    setDateTimeCalled = datetime != nullptr;
    if (datetime != nullptr) {
      lastSetTm = *datetime;
      dateTime.date.year = static_cast<std::int16_t>(datetime->tm_year + 1900);
      dateTime.date.month = static_cast<std::int8_t>(datetime->tm_mon + 1);
      dateTime.date.date = static_cast<std::int8_t>(datetime->tm_mday);
      dateTime.date.weekDay = static_cast<std::int8_t>(datetime->tm_wday);
      dateTime.time.hours = static_cast<std::int8_t>(datetime->tm_hour);
      dateTime.time.minutes = static_cast<std::int8_t>(datetime->tm_min);
      dateTime.time.seconds = static_cast<std::int8_t>(datetime->tm_sec);
    }
  }

  bool getVoltLow() const {
    return voltLow;
  }
};

struct FakeI2C {
  bool enabled = false;
  bool scanFound = false;
  bool failNextRead = false;
  std::array<std::uint8_t, 256> registers{};
  std::array<std::uint8_t, 6> nextReadBuffer{};
  std::uint8_t lastAddress = 0;
  bool lastReadMode = false;
  std::uint32_t lastFrequency = 0;
  int startCalls = 0;
  int stopCalls = 0;
  std::vector<std::uint8_t> writtenBytes;

  bool isEnabled() const {
    return enabled;
  }

  bool scanID(std::uint8_t, std::uint32_t) const {
    return scanFound;
  }

  bool start(std::uint8_t address, bool read, std::uint32_t frequency) {
    ++startCalls;
    lastAddress = address;
    lastReadMode = read;
    lastFrequency = frequency;
    return enabled;
  }

  bool write(std::uint8_t value) {
    if (!enabled) {
      return false;
    }
    writtenBytes.push_back(value);
    return true;
  }

  bool stop() {
    ++stopCalls;
    return enabled;
  }

  bool read(std::uint8_t* buffer, std::size_t length, bool) {
    if (!enabled || failNextRead) {
      failNextRead = false;
      return false;
    }

    const std::size_t copyLength = std::min(length, nextReadBuffer.size());
    for (std::size_t i = 0; i < copyLength; ++i) {
      buffer[i] = nextReadBuffer[i];
    }
    return true;
  }

  std::uint8_t readRegister8(std::uint8_t, std::uint8_t reg, std::uint32_t) {
    if (!enabled || failNextRead) {
      failNextRead = false;
      return 0xFF;
    }
    return registers[reg];
  }

  bool writeRegister8(std::uint8_t, std::uint8_t reg, std::uint8_t value, std::uint32_t) {
    if (!enabled) {
      return false;
    }
    registers[reg] = value;
    return true;
  }

  bool bitOn(std::uint8_t, std::uint8_t reg, std::uint8_t mask, std::uint32_t) {
    if (!enabled) {
      return false;
    }
    registers[reg] |= mask;
    return true;
  }

  bool bitOff(std::uint8_t, std::uint8_t reg, std::uint8_t mask, std::uint32_t) {
    if (!enabled) {
      return false;
    }
    registers[reg] &= static_cast<std::uint8_t>(~mask);
    return true;
  }
};

struct FakePrintedText {
  int x = 0;
  int y = 0;
  int size = 1;
  FakeFontKind fontKind = FakeFontKind::kDefault;
  std::string text;
  std::uint32_t color = TFT_BLACK;
  std::uint32_t background = TFT_WHITE;
  int datum = 0;
};

struct FakeRect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  std::uint32_t color = 0;
};

struct FakePngDraw {
  std::string path;
  int x = 0;
  int y = 0;
  int maxWidth = 0;
  int maxHeight = 0;
  int datum = 0;
};

struct FakeSpritePush {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

enum class datum_t {
  top_left = 0,
  middle_center = 1,
};

enum class textdatum_t {
  top_left = 0,
  top_center = 1,
  top_right = 2,
  middle_left = 4,
  middle_center = 5,
  middle_right = 6,
  bottom_left = 8,
  bottom_center = 9,
  bottom_right = 10,
};

struct FakeDisplay {
  struct FakeEpdEvent {
    enum class Type {
      SetMode,
      Wakeup,
      Clear,
      WaitDisplay,
    };

    Type type = Type::Wakeup;
    epd_mode_t mode = epd_mode_t::epd_fast;
    std::uint32_t color = 0;
  };

  int rotation = 0;
  int cursorX = 0;
  int cursorY = 0;
  int textSize = 1;
  FakeFontKind fontKind = FakeFontKind::kDefault;
  int widthPixels = 400;
  int heightPixels = 600;
  bool textWrap = true;
  bool loadFontSucceeds = true;
  std::uint32_t textColor = TFT_BLACK;
  std::uint32_t textBackground = TFT_WHITE;
  std::uint32_t fillScreenColor = TFT_WHITE;
  textdatum_t textDatum = textdatum_t::top_left;
  std::vector<FakePrintedText> prints;
  std::vector<FakeRect> rects;
  std::vector<FakePngDraw> pngDraws;
  std::vector<FakeSpritePush> spritePushes;
  int directRectCount = 0;
  int directPngDrawCount = 0;
  int waitDisplayCount = 0;
  int sleepCount = 0;
  int wakeupCount = 0;
  int clearCount = 0;
  std::vector<std::uint32_t> clearColors;
  std::vector<epd_mode_t> epdModes;
  std::vector<FakeEpdEvent> epdEvents;

  void sleep() {
    ++sleepCount;
  }

  void wakeup() {
    ++wakeupCount;
    epdEvents.push_back({FakeEpdEvent::Type::Wakeup, epd_mode_t::epd_fast, 0});
  }

  void clear(std::uint32_t color) {
    ++clearCount;
    clearColors.push_back(color);
    fillScreenColor = color;
    epdEvents.push_back({FakeEpdEvent::Type::Clear, epd_mode_t::epd_fast, color});
  }

  static int lineHeightFor(FakeFontKind kind) {
    if (kind == FakeFontKind::kDefault) {
      return 8;
    }
    if (kind == FakeFontKind::kDeviceMetric) {
      return 28;
    }
    if (kind == FakeFontKind::kDeviceLargeDate) {
      return 78;
    }
    return 14;
  }

  void setRotation(int value) {
    rotation = value;
  }

  int getRotation() const {
    return rotation;
  }

  int width() const {
    return widthPixels;
  }

  int height() const {
    return heightPixels;
  }

  void fillScreen(std::uint32_t color) {
    fillScreenColor = color;
  }

  void setTextColor(std::uint32_t fg, std::uint32_t bg) {
    textColor = fg;
    textBackground = bg;
  }

  void setTextWrap(bool value) {
    textWrap = value;
  }

  void setTextSize(int value) {
    textSize = value;
  }

  void setFont(const void* font) {
    fontKind = font == static_cast<const void*>(&fonts::efontCN_14)
        ? FakeFontKind::kChinese
        : FakeFontKind::kDefault;
  }

  void setCursor(int x, int y) {
    cursorX = x;
    cursorY = y;
  }

  void print(const char* text) {
    prints.push_back({cursorX, cursorY, textSize, fontKind, text != nullptr ? text : "", textColor, textBackground});
    cursorX += textWidth(text);
  }

  void println(const char* text) {
    print(text);
    cursorX = 0;
    cursorY += lineHeightFor(fontKind) * textSize;
  }

  void println() {
    cursorX = 0;
    cursorY += lineHeightFor(fontKind) * textSize;
  }

  static int utf8CodePointCount(const char* text) {
    if (text == nullptr) {
      return 0;
    }

    int count = 0;
    for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text); *cursor != 0; ++cursor) {
      if ((*cursor & 0xC0) != 0x80) {
        ++count;
      }
    }
    return count;
  }

  static int codePointWidthFor(FakeFontKind kind, unsigned char leadByte) {
    if (kind == FakeFontKind::kDefault) {
      return 8;
    }

    if (kind == FakeFontKind::kDeviceDefault) {
      return leadByte < 0x80 || (leadByte & 0xE0) == 0xC0 ? 10 : 20;
    }

    if (kind == FakeFontKind::kDeviceMetric) {
      return leadByte < 0x80 || (leadByte & 0xE0) == 0xC0 ? 14 : 28;
    }

    if (kind == FakeFontKind::kDeviceLargeDate) {
      return 39;
    }

    if (leadByte < 0x80 || (leadByte & 0xE0) == 0xC0) {
      return 9;
    }

    return 12;
  }

  bool loadFont(const std::uint8_t* font) {
    if (!loadFontSucceeds) {
      fontKind = FakeFontKind::kDefault;
      return false;
    }
    if (font == nullptr) {
      fontKind = FakeFontKind::kDefault;
    } else if (font == homedeck::generated::kConfigPortalFontVlw) {
      fontKind = FakeFontKind::kConfigPortal;
    } else if (font == homedeck::generated::kDeviceLargeDateFontVlw) {
      fontKind = FakeFontKind::kDeviceLargeDate;
    } else if (font == homedeck::generated::kDeviceMetricFontVlw) {
      fontKind = FakeFontKind::kDeviceMetric;
    } else if (font == homedeck::generated::kDeviceFontVlw) {
      fontKind = FakeFontKind::kDeviceDefault;
    } else {
      fontKind = FakeFontKind::kDeviceDefault;
    }
    return true;
  }

  void unloadFont() {
    fontKind = FakeFontKind::kDefault;
  }

  template <typename TFs>
  bool drawPngFile(
      TFs&,
      const char* path,
      int x,
      int y,
      int maxWidth,
      int maxHeight,
      int,
      int,
      float,
      float,
      datum_t datum) {
    ++directPngDrawCount;
    pngDraws.push_back({path != nullptr ? path : "", x, y, maxWidth, maxHeight, static_cast<int>(datum)});
    return true;
  }

  int textWidth(const char* text) const {
    if (text == nullptr) {
      return 0;
    }

    int width = 0;
    for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text); *cursor != 0; ++cursor) {
      if ((*cursor & 0xC0) != 0x80) {
        width += codePointWidthFor(fontKind, *cursor) * textSize;
      }
    }
    return width;
  }

  int fontHeight() const {
    return lineHeightFor(fontKind) * textSize;
  }

  void getTextBounds(const char* text, int16_t, int16_t,
                     int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
    if (text == nullptr || x1 == nullptr || y1 == nullptr || w == nullptr || h == nullptr) {
      return;
    }
    *w = textWidth(text);
    *h = lineHeightFor(fontKind) * textSize;
    *x1 = 0;
    *y1 = -*h;
  }

  void drawRect(int x, int y, int w, int h, std::uint32_t color) {
    rects.push_back({x, y, w, h, color});
  }

  void fillRect(int x, int y, int w, int h, std::uint32_t color) {
    ++directRectCount;
    rects.push_back({x, y, w, h, color});
  }

  void drawFastHLine(int32_t x, int32_t y, int32_t w, std::uint32_t color) {
    rects.push_back({x, y, w, 1, color});
  }

  void startWrite() {
  }

  void endWrite() {
  }

  void waitDisplay() {
    ++waitDisplayCount;
    epdEvents.push_back({FakeEpdEvent::Type::WaitDisplay, epd_mode_t::epd_fast, 0});
  }

  void setTextDatum(textdatum_t value) {
    textDatum = value;
  }

  void drawString(const char* text, int x, int y) {
    cursorX = x;
    cursorY = y;
    prints.push_back(
        {x, y, textSize, fontKind, text != nullptr ? text : "", textColor, textBackground, static_cast<int>(textDatum)});
  }

  void setEpdMode(epd_mode_t mode) {
    epdModes.push_back(mode);
    epdEvents.push_back({FakeEpdEvent::Type::SetMode, mode, 0});
  }
};

// FakeCanvas mirrors M5Canvas API. It records drawn content into the
// parent FakeDisplay so existing tests can inspect what was rendered.
struct FakeCanvas {
  FakeDisplay* parent = nullptr;
  int spriteWidth = 0;
  int spriteHeight = 0;
  bool created = false;
  int cursorX = 0;
  int cursorY = 0;
  int textSize = 1;
  FakeFontKind fontKind = FakeFontKind::kDefault;
  int lineStartX = 0;
  int colorDepth = 16;
  std::uint32_t textColor = TFT_BLACK;
  std::uint32_t textBackground = TFT_WHITE;
  textdatum_t textDatum = textdatum_t::top_left;
  std::vector<FakePrintedText> prints;
  std::vector<FakeRect> rects;
  std::vector<FakePngDraw> pngDraws;

  explicit FakeCanvas(FakeDisplay* display) : parent(display) {
  }

  void createSprite(int w, int h) {
    spriteWidth = w;
    spriteHeight = h;
    created = true;
  }

  void deleteSprite() {
    created = false;
  }

  int width() const {
    return spriteWidth;
  }

  int height() const {
    return spriteHeight;
  }

  void fillSprite(std::uint32_t color) {
    if (parent != nullptr) {
      parent->fillScreenColor = color;
    }
    prints.clear();
    rects.clear();
    pngDraws.clear();
  }

  void setColorDepth(int depth) {
    colorDepth = depth;
  }

  void setTextColor(std::uint32_t fg, std::uint32_t bg) {
    textColor = fg;
    textBackground = bg;
    if (parent != nullptr) {
      parent->textColor = fg;
      parent->textBackground = bg;
    }
  }

  void setTextColor(std::uint32_t fg) {
    textColor = fg;
    if (parent != nullptr) {
      parent->textColor = fg;
    }
  }

  void fillCircle(int x, int y, int r, std::uint32_t color) {
    rects.push_back({x - r, y - r, r * 2, r * 2, color});
  }


  void setTextWrap(bool value) {
    if (parent != nullptr) {
      parent->textWrap = value;
    }
  }

  void setTextSize(int value) {
    textSize = value;
    if (parent != nullptr) {
      parent->textSize = value;
    }
  }

  void setFont(const void* font) {
    fontKind = font == static_cast<const void*>(&fonts::efontCN_14)
        ? FakeFontKind::kChinese
        : FakeFontKind::kDefault;
    if (parent != nullptr) {
      parent->fontKind = fontKind;
    }
  }

  bool loadFont(const std::uint8_t* font) {
    if (parent == nullptr || !parent->loadFont(font)) {
      fontKind = FakeFontKind::kDefault;
      return false;
    }
    fontKind = parent->fontKind;
    return true;
  }

  void unloadFont() {
    fontKind = FakeFontKind::kDefault;
    if (parent != nullptr) {
      parent->fontKind = FakeFontKind::kDefault;
    }
  }

  void setCursor(int x, int y) {
    cursorX = x;
    cursorY = y;
    lineStartX = x;
    if (parent != nullptr) {
      parent->cursorX = x;
      parent->cursorY = y;
    }
  }

  void print(const char* text) {
    prints.push_back(
        {cursorX, cursorY, textSize, fontKind, text != nullptr ? text : "", textColor, textBackground, static_cast<int>(textDatum)});
    cursorX += textWidth(text);
  }

  void println(const char* text) {
    print(text);
    cursorX = lineStartX;
    cursorY += FakeDisplay::lineHeightFor(fontKind) * textSize;
    if (parent != nullptr) {
      parent->cursorX = cursorX;
      parent->cursorY = cursorY;
    }
  }

  void println() {
    cursorX = lineStartX;
    cursorY += FakeDisplay::lineHeightFor(fontKind) * textSize;
    if (parent != nullptr) {
      parent->cursorX = cursorX;
      parent->cursorY = cursorY;
    }
  }

  int textWidth(const char* text) const {
    if (text == nullptr) {
      return 0;
    }

    int width = 0;
    for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text); *cursor != 0; ++cursor) {
      if ((*cursor & 0xC0) != 0x80) {
        width += FakeDisplay::codePointWidthFor(fontKind, *cursor) * textSize;
      }
    }
    return width;
  }

  int fontHeight() const {
    return FakeDisplay::lineHeightFor(fontKind) * textSize;
  }

  void getTextBounds(const char* text, int16_t, int16_t,
                     int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
    if (text == nullptr || x1 == nullptr || y1 == nullptr || w == nullptr || h == nullptr) {
      return;
    }
    *w = textWidth(text);
    *h = FakeDisplay::lineHeightFor(fontKind) * textSize;
    *x1 = 0;
    *y1 = -*h;
  }

  void drawRect(int x, int y, int w, int h, std::uint32_t color) {
    rects.push_back({x, y, w, h, color});
  }

  void fillRect(int x, int y, int w, int h, std::uint32_t color) {
    rects.push_back({x, y, w, h, color});
  }

  void drawFastHLine(int32_t x, int32_t y, int32_t w, std::uint32_t color) {
    rects.push_back({x, y, w, 1, color});
  }

  void setTextDatum(textdatum_t value) {
    textDatum = value;
    if (parent != nullptr) {
      parent->textDatum = value;
    }
  }

  void drawString(const char* text, int x, int y) {
    cursorX = x;
    cursorY = y;
    prints.push_back(
        {x, y, textSize, fontKind, text != nullptr ? text : "", textColor, textBackground, static_cast<int>(textDatum)});
  }

  void pushSprite(int x, int y) {
    if (parent == nullptr) {
      return;
    }
    parent->spritePushes.push_back({x, y, spriteWidth, spriteHeight});
    parent->prints.insert(parent->prints.end(), prints.begin(), prints.end());
    parent->rects.insert(parent->rects.end(), rects.begin(), rects.end());
    parent->pngDraws.insert(parent->pngDraws.end(), pngDraws.begin(), pngDraws.end());
  }

  template <typename TFs>
  bool drawPngFile(
      TFs& fs,
      const char* path,
      int x,
      int y,
      int maxWidth,
      int maxHeight,
      int c,
      int d,
      float e,
      float f,
      datum_t datum) {
    (void)fs;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    pngDraws.push_back({path != nullptr ? path : "", x, y, maxWidth, maxHeight, static_cast<int>(datum)});
    return true;
  }
};

using M5Canvas = FakeCanvas;

struct FakeButton {
  bool pressed = false;
  bool clicked = false;
  int clickCount = 0;
  bool decideClickCount = false;

  bool isPressed() const {
    return pressed;
  }
  bool wasClicked() {
    if (clicked) {
      clicked = false;
      return true;
    }
    return false;
  }
  bool wasDecideClickCount() {
    if (decideClickCount) {
      decideClickCount = false;
      return true;
    }
    return false;
  }
  int getClickCount() const {
    return clickCount;
  }
};

struct FakeLed {
  uint8_t brightness = 0;
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  bool displayed = false;

  void setBrightness(uint8_t val) { brightness = val; }
  void setAllColor(uint8_t red, uint8_t green, uint8_t blue) {
    r = red;
    g = green;
    b = blue;
  }
  void display() { displayed = true; }
};

struct FakePower {
  int16_t vbus_voltage = 0;
  int32_t battery_level = 50;

  int16_t getVBUSVoltage() { return vbus_voltage; }
  int32_t getBatteryLevel() { return battery_level; }
  void setLed(uint8_t) {}
  void deepSleep(std::uint64_t micro_seconds, bool touch_wakeup = true) {
    (void)micro_seconds;
    (void)touch_wakeup;
    extern void esp_deep_sleep_start(void);
    esp_deep_sleep_start();
  }
};

struct FakeM5Config {
  bool clear_display = true;
  bool output_power = true;
  bool pmic_button = true;
  bool internal_rtc = true;
  bool internal_imu = true;
  bool internal_spk = true;
  bool internal_mic = true;
  uint8_t led_brightness = 0;
  int serial_baudrate = 0;
};

struct FakeM5Global {
  FakeRtc Rtc;
  FakeI2C In_I2C;
  FakeDisplay Display;
  FakeButton BtnA;
  FakeButton BtnB;
  FakeButton BtnC;
  FakeLed Led;
  FakePower Power;
  int updateCount = 0;

  FakeM5Config config() const {
    return FakeM5Config{};
  }

  void begin() {
  }

  void begin(const FakeM5Config&) {
  }

  void update() {
    ++updateCount;
  }
};

inline FakeM5Global M5;
