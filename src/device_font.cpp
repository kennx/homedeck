#include "device_font.h"

#include <cstdint>

#if !defined(UNIT_TEST)
#include "generated/device_font_vlw.h"
#endif

namespace homedeck::device_font {
namespace {

#if defined(UNIT_TEST)
const std::uint8_t kTestDeviceFontVlw[] = {14};
const std::uint8_t kTestMetricFontVlw[] = {28};
const std::uint8_t kTestTimeFontVlw[] = {42};
#endif

}  // namespace

bool apply(M5Canvas& canvas, Role role) {
#if defined(UNIT_TEST)
  switch (role) {
    case Role::kMetricValue:
      return canvas.loadFont(kTestMetricFontVlw);
    case Role::kTime:
      return canvas.loadFont(kTestTimeFontVlw);
    case Role::kBody:
      break;
  }
  return canvas.loadFont(kTestDeviceFontVlw);
#else
  switch (role) {
    case Role::kMetricValue:
      return canvas.loadFont(generated::kDeviceMetricFontVlw);
    case Role::kTime:
      return canvas.loadFont(generated::kDeviceTimeFontVlw);
    case Role::kBody:
      break;
  }
  return canvas.loadFont(generated::kDeviceFontVlw);
#endif
}

bool applyDefault(M5Canvas& canvas) {
  return apply(canvas, Role::kBody);
}

}  // namespace homedeck::device_font
