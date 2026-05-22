#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <string>
#include <vector>

constexpr std::uint32_t TFT_BLACK = 0x00000000u;
constexpr std::uint32_t TFT_WHITE = 0x00FFFFFFu;

namespace fonts {
inline const int efontCN_14 = 0;
}  // namespace fonts

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
    return true;
  }

  void setSystemTimeFromRtc() {
    setSystemTimeFromRtcCalled = true;
  }

  void setDateTime(const std::tm* datetime) {
    setDateTimeCalled = datetime != nullptr;
    if (datetime != nullptr) {
      lastSetTm = *datetime;
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
  std::array<std::uint8_t, 6> nextReadBuffer{};

  bool isEnabled() const {
    return enabled;
  }

  bool scanID(std::uint8_t, std::uint32_t) const {
    return scanFound;
  }

  bool start(std::uint8_t, bool, std::uint32_t) {
    return enabled;
  }

  bool write(std::uint8_t) {
    return enabled;
  }

  bool stop() {
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
};

struct FakePrintedText {
  int x = 0;
  int y = 0;
  int size = 1;
  std::string text;
};

struct FakeRect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  std::uint32_t color = 0;
};

struct FakeDisplay {
  int rotation = 0;
  int cursorX = 0;
  int cursorY = 0;
  int textSize = 1;
  int widthPixels = 400;
  int heightPixels = 600;
  bool textWrap = true;
  std::uint32_t textColor = TFT_BLACK;
  std::uint32_t textBackground = TFT_WHITE;
  std::uint32_t fillScreenColor = TFT_WHITE;
  std::vector<FakePrintedText> prints;
  std::vector<FakeRect> rects;

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

  void setFont(const void*) {
  }

  void setCursor(int x, int y) {
    cursorX = x;
    cursorY = y;
  }

  void print(const char* text) {
    prints.push_back({cursorX, cursorY, textSize, text != nullptr ? text : ""});
  }

  void println(const char* text) {
    print(text);
  }

  void println() {
  }

  int textWidth(const char* text) const {
    return static_cast<int>(std::strlen(text != nullptr ? text : "")) * textSize * 8;
  }

  void drawRect(int x, int y, int w, int h, std::uint32_t color) {
    rects.push_back({x, y, w, h, color});
  }

  void fillRect(int, int, int, int, std::uint32_t) {
  }

  void startWrite() {
  }

  void endWrite() {
  }

  void waitDisplay() {
  }

  void setEpdMode(int) {
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
  }

  void setTextColor(std::uint32_t fg, std::uint32_t bg) {
    if (parent != nullptr) {
      parent->textColor = fg;
      parent->textBackground = bg;
    }
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

  void setFont(const void*) {
  }

  void setCursor(int x, int y) {
    cursorX = x;
    cursorY = y;
    if (parent != nullptr) {
      parent->cursorX = x;
      parent->cursorY = y;
    }
  }

  void print(const char* text) {
    if (parent != nullptr) {
      parent->cursorX = cursorX;
      parent->cursorY = cursorY;
      parent->prints.push_back({cursorX, cursorY, textSize, text != nullptr ? text : ""});
    }
  }

  void println(const char* text) {
    print(text);
  }

  void println() {
  }

  int textWidth(const char* text) const {
    if (parent != nullptr) {
      return parent->textWidth(text);
    }
    return static_cast<int>(std::strlen(text != nullptr ? text : "")) * textSize * 8;
  }

  void drawRect(int x, int y, int w, int h, std::uint32_t color) {
    if (parent != nullptr) {
      parent->rects.push_back({x, y, w, h, color});
    }
  }

  void fillRect(int, int, int, int, std::uint32_t) {
  }

  void pushSprite(int, int) {
  }
};

using M5Canvas = FakeCanvas;

struct FakeButton {
  bool pressed = false;
  bool isPressed() const {
    return pressed;
  }
};

struct FakeM5Global {
  FakeRtc Rtc;
  FakeI2C In_I2C;
  FakeDisplay Display;
  FakeButton BtnA;
  FakeButton BtnB;

  void begin() {
  }

  void update() {
  }
};

extern FakeM5Global M5;
