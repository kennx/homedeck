#include "home_renderer.h"

#include "device_font.h"

#include <M5Unified.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace {

constexpr int kMarginX = 20;
constexpr int kTimeY = 24;
constexpr int kDateY = 106;
constexpr int kLunarY = 144;
constexpr int kSolarTermY = 174;
constexpr int kHolidayY = 204;
constexpr int kMetricBoxY = 244;
constexpr int kMetricBoxWidth = 170;
constexpr int kMetricBoxHeight = 92;
constexpr int kHumidityBoxX = 210;
constexpr int kEventsTitleY = 362;
constexpr int kEventListY = 396;
constexpr int kEventRowHeight = 32;
constexpr int kEventTimeWidth = 92;
constexpr int kEventFooterY = 528;
constexpr int kStatusY = 566;
constexpr int kBodyTextSize = 1;
constexpr int kMetricValueTextSize = 2;
constexpr int kTimeTextSize = 3;

void trimLastUtf8CodePoint(std::string* text) {
  if (text == nullptr || text->empty()) {
    return;
  }

  std::size_t index = text->size();
  do {
    --index;
  } while (index > 0 && ((*text)[index] & 0xC0) == 0x80);

  text->erase(index);
}

std::string fitText(M5Canvas& canvas, int size, int maxWidth, const std::string& text) {
  canvas.setTextSize(size);
  if (maxWidth <= 0 || text.empty()) {
    return "";
  }

  if (canvas.textWidth(text.c_str()) <= maxWidth) {
    return text;
  }

  std::string fitted = text;
  constexpr const char* kEllipsis = "...";
  const int ellipsisWidth = canvas.textWidth(kEllipsis);
  if (ellipsisWidth > maxWidth) {
    while (!fitted.empty() && canvas.textWidth(fitted.c_str()) > maxWidth) {
      trimLastUtf8CodePoint(&fitted);
    }
    return fitted;
  }

  while (!fitted.empty()) {
    trimLastUtf8CodePoint(&fitted);
    const std::string candidate = fitted + kEllipsis;
    if (canvas.textWidth(candidate.c_str()) <= maxWidth) {
      return candidate;
    }
  }

  return kEllipsis;
}

void drawText(
    M5Canvas& canvas,
    int x,
    int y,
    int size,
    const std::string& text,
    int maxWidth) {
  canvas.setTextSize(size);
  canvas.setCursor(x, y);
  canvas.print(fitText(canvas, size, maxWidth, text).c_str());
}

void drawMetricBox(M5Canvas& canvas, int x, int y, const char* label, const std::string& value) {
  canvas.drawRect(x, y, kMetricBoxWidth, kMetricBoxHeight, TFT_BLACK);

  drawText(canvas, x + 12, y + 12, kBodyTextSize, label, kMetricBoxWidth - 24);
  drawText(canvas, x + 12, y + 44, kMetricValueTextSize, value, kMetricBoxWidth - 24);
}

void drawEventRow(M5Canvas& canvas, int x, int y, const homedeck::EventRow& row) {
  if (row.timeText.empty()) {
    drawText(canvas, x, y, kBodyTextSize, row.titleText, canvas.width() - x - kMarginX);
    return;
  }

  drawText(canvas, x, y, kBodyTextSize, row.timeText, kEventTimeWidth - 8);
  drawText(
      canvas,
      x + kEventTimeWidth,
      y,
      kBodyTextSize,
      row.titleText,
      canvas.width() - (x + kEventTimeWidth) - kMarginX);
}

}  // namespace

void HomeRenderer::render(const homedeck::HomeViewModel& model) {
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  canvas.fillSprite(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setTextWrap(false);
  if (!homedeck::device_font::applyDefault(canvas)) {
    canvas.setFont(nullptr);
  }

  const int pageWidth = canvas.width();
  const int contentWidth = pageWidth - kMarginX * 2;

  drawText(
      canvas,
      kMarginX,
      kTimeY,
      kTimeTextSize,
      model.timeText.empty() ? "--:--" : model.timeText,
      contentWidth);
  drawText(canvas, kMarginX, kDateY, kBodyTextSize, model.dateText, contentWidth);
  drawText(canvas, kMarginX, kLunarY, kBodyTextSize, model.lunarText, contentWidth);
  drawText(canvas, kMarginX, kSolarTermY, kBodyTextSize, model.solarTermText, contentWidth);
  drawText(canvas, kMarginX, kHolidayY, kBodyTextSize, model.holidayText, contentWidth);

  drawMetricBox(canvas, kMarginX, kMetricBoxY, "温度", model.temperatureText);
  drawMetricBox(canvas, kHumidityBoxX, kMetricBoxY, "湿度", model.humidityText);

  drawText(canvas, kMarginX, kEventsTitleY, kBodyTextSize, "今日日程", contentWidth);

  const std::uint32_t visibleCount = std::min<std::uint32_t>(
      model.eventCount,
      static_cast<std::uint32_t>(model.eventRows.size()));
  if (visibleCount == 0) {
    if (!model.eventRows[0].titleText.empty()) {
      drawEventRow(canvas, kMarginX, kEventListY, model.eventRows[0]);
    }
  } else {
    for (std::uint32_t index = 0; index < visibleCount; ++index) {
      drawEventRow(
          canvas,
          kMarginX,
          kEventListY + static_cast<int>(index) * kEventRowHeight,
          model.eventRows[index]);
    }
  }

  if (model.eventCount > visibleCount) {
    drawText(
        canvas,
        kMarginX,
        kEventFooterY,
        kBodyTextSize,
        "还有 " + std::to_string(model.eventCount - visibleCount) + " 项",
        contentWidth);
  }

  drawText(
      canvas,
      kMarginX,
      kStatusY,
      kBodyTextSize,
      homedeck::buildStatusText(model),
      contentWidth);

  M5.Display.startWrite();
  canvas.pushSprite(0, 0);
  M5.Display.endWrite();
  M5.Display.waitDisplay();
  canvas.deleteSprite();
}
