#include "setup_renderer.h"

#include <M5Unified.h>
#include <qrcode.h>

#include <string>

#include "homedeck/setup_page.h"

namespace {

void drawQrCode(M5Canvas& canvas, const std::string& payload) {
  QRCode qrcode;
  uint8_t modules[qrcode_getBufferSize(4)];
  qrcode_initText(&qrcode, modules, 4, ECC_LOW, payload.c_str());

  constexpr int kScale = 6;
  constexpr int kQuietZone = 2;
  const int qrSize = qrcode.size;
  const int totalSize = (qrSize + kQuietZone * 2) * kScale;
  const int originX = canvas.width() - totalSize - 20;
  const int originY = 120;

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
  canvas.setFont(&fonts::efontCN_14);
  canvas.setTextSize(2);
  canvas.setCursor(20, 20);
  canvas.println("HomeDeck 配网");
  canvas.println();
  canvas.println("1. 连接开放热点");
  canvas.println(apSsid);
  canvas.println();
  canvas.println("2. 打开 192.168.4.1");
  canvas.print("当前热点 IP: ");
  canvas.println(ipText);

  drawQrCode(canvas, homedeck::buildWifiQrPayload(apSsid));

  M5.Display.startWrite();
  canvas.pushSprite(0, 0);
  M5.Display.endWrite();
  M5.Display.waitDisplay();
  canvas.deleteSprite();
}
