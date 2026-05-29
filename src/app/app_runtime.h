#pragma once

#include <ctime>
#include <string>

#include "app/boot_controller.h"

namespace homedeck {

void appSetup();
void appLoop();
void enterHomeDeepSleep(const HomeSleepRequest& request);
#ifdef UNIT_TEST
bool syncNtpForTest(
    const std::string& posixTimezone,
    const std::string& ntpServer,
    time_t* syncedUnix);
bool writeRtcUtcForTest(time_t unixTime);
void prepareEpdAfterWakeupForTest();
void initRgbLedForTest();
void shutdownRgbLedForSleepForTest();
#endif

}  // namespace homedeck
