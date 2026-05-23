#pragma once

#include <M5Unified.h>

namespace homedeck::device_font {

enum class Role {
  kBody,
  kMetricValue,
  kTime,
};

bool apply(M5Canvas& canvas, Role role);
bool applyDefault(M5Canvas& canvas);

}  // namespace homedeck::device_font
