#pragma once

#include <ctime>
#include <string>

#include "boot_controller.h"

namespace homedeck {

void appSetup();
void appLoop();
void enterHomeDeepSleep(const HomeSleepRequest& request);
#ifdef UNIT_TEST
bool syncNtpForTest(
    const std::string& posixTimezone,
    const std::string& ntpServer,
    time_t* syncedUnix);
#endif

}  // namespace homedeck
