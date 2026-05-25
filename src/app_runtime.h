#pragma once

#include "boot_controller.h"

namespace homedeck {

void appSetup();
void appLoop();
void enterHomeDeepSleep(const HomeSleepRequest& request);

}  // namespace homedeck
