#pragma once
#include <M5Unified.h>
#include <ctime>
#include <string>

namespace homedeck {
M5Canvas& sprite();
void prepareScreen(M5Canvas& canvas);
void pushScreen(M5Canvas& canvas);
std::string formatCurrentTimeHHMM();
}  // namespace homedeck
