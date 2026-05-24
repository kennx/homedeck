#include "home_renderer.h"

#include <LittleFS.h>
#include <M5Unified.h>
#include <qrcode.h>

#include <cstdint>
#include <string>
#include <vector>

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

}  // namespace

void HomeRenderer::render() {
  M5Canvas canvas(&M5.Display);
  prepareScreen(canvas);
  drawLogo(canvas, (M5.Display.height() - kLogoHeight) / 2);
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
