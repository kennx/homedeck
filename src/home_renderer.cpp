#include "home_renderer.h"

#include <M5Unified.h>

#include <string>

namespace homedeck {

void HomeRenderer::render() {
  M5.Display.setRotation(0);
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString("HomeDeck", M5.Display.width() / 2, M5.Display.height() / 2);
}

void HomeRenderer::renderConfigPortal(const std::string& apSsid, const std::string& ipAddress) {
  M5.Display.setRotation(0);
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(textdatum_t::middle_center);

  const int centerX = M5.Display.width() / 2;
  const int centerY = M5.Display.height() / 2;
  const std::string apLine = std::string("AP: ") + apSsid;
  const std::string ipLine = std::string("IP: ") + ipAddress;

  M5.Display.setTextSize(2);
  M5.Display.drawString("Config Mode", centerX, centerY - 48);
  M5.Display.setTextSize(1);
  M5.Display.drawString(apLine.c_str(), centerX, centerY);
  M5.Display.drawString(ipLine.c_str(), centerX, centerY + 28);
}

}  // namespace homedeck
