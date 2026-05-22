#include "device_font.h"

#include <cstdint>

#if !defined(UNIT_TEST)
#include "generated/device_font_vlw.h"
#endif

namespace homedeck::device_font {
namespace {

#if defined(UNIT_TEST)
const std::uint8_t kTestDeviceFontVlw[] = {0};
#endif

}  // namespace

bool applyDefault(M5Canvas& canvas) {
#if defined(UNIT_TEST)
  return canvas.loadFont(kTestDeviceFontVlw);
#else
  return canvas.loadFont(generated::kDeviceFontVlw);
#endif
}

}  // namespace homedeck::device_font
