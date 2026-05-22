#include "setup_renderer.h"

#include <M5Unified.h>
#include <qrcode.h>

#include <string>

#include "device_font.h"
#include "homedeck/setup_page.h"

namespace {

constexpr int kLeft = 20;
constexpr int kTop = 20;
constexpr int kTextSize = 2;
constexpr int kNormalLineHeight = 40;
constexpr int kValueLineHeight = 40;
constexpr int kMixedLineHeight = 40;
constexpr int kQrTopGap = 8;

void drawQrCode(M5Canvas& canvas, const std::string& payload, int originY) {
  QRCode qrcode;
  uint8_t modules[qrcode_getBufferSize(4)];
  qrcode_initText(&qrcode, modules, 4, ECC_LOW, payload.c_str());

  constexpr int kScale = 6;
  constexpr int kQuietZone = 2;
  const int qrSize = qrcode.size;
  const int totalSize = (qrSize + kQuietZone * 2) * kScale;
  const int originX = canvas.width() - totalSize - 20;

  canvas.fillRect(originX, originY, totalSize, totalSize, TFT_WHITE);

  for (int y = 0; y < qrSize; ++y) {
    for (int x = 0; x < qrSize; ++x) {
      if (!qrcode_getModule(&qrcode, x, y)) {
        continue;
      }

      const int pixelX = originX + (x + kQuietZone) * kScale;
      const int pixelY = originY + (y + kQuietZone) * kScale;
      canvas.fillRect(pixelX, pixelY, kScale, kScale, TFT_BLACK);
    }
  }
}

}  // namespace

void SetupRenderer::render(const char* apSsid, const char* ipText) {
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  canvas.fillSprite(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setTextWrap(false);
  if (!homedeck::device_font::applyDefault(canvas)) {
    canvas.setFont(nullptr);
  }
  canvas.setTextSize(kTextSize);

  int y = kTop;

  canvas.setCursor(kLeft, y);
  canvas.print("HomeDeck 配网");
  y += kNormalLineHeight;

  canvas.setCursor(kLeft, y);
  canvas.print("1. 连接开放热点");
  y += kNormalLineHeight;

  canvas.setCursor(kLeft, y);
  canvas.print(apSsid);
  y += kValueLineHeight;

  canvas.setCursor(kLeft, y);
  canvas.print("2. 打开 ");

  const int step2ValueX = kLeft + canvas.textWidth("2. 打开 ");
  canvas.setCursor(step2ValueX, y);
  canvas.print(ipText);
  y += kMixedLineHeight;

  canvas.setCursor(kLeft, y);
  canvas.print("当前热点 IP: ");

  canvas.print(ipText);

  drawQrCode(canvas, homedeck::buildWifiQrPayload(apSsid), y + kMixedLineHeight + kQrTopGap);

  M5.Display.startWrite();
  canvas.pushSprite(0, 0);
  M5.Display.endWrite();
  M5.Display.waitDisplay();
  canvas.deleteSprite();
}
