#include "system/render_context.h"

namespace homedeck {

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

std::string formatCurrentTimeHHMM() {
  std::time_t now = std::time(nullptr);
  std::tm buf{};
  std::tm* local = localtime_r(&now, &buf);
  char timeStr[6] = {};
  if (local != nullptr) {
    std::snprintf(timeStr, sizeof(timeStr), "%02d:%02d", local->tm_hour, local->tm_min);
  }
  return std::string(timeStr);
}

}  // namespace homedeck
