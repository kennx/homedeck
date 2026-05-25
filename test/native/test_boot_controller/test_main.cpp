#include <unity.h>

#include <cstdlib>
#include <ctime>
#include <vector>

#include "boot_controller.h"

namespace {

struct Fixture {
  bool configured = false;
  bool forceConfig = false;
  bool portalStarted = false;
  bool portalHandled = false;
  bool homeRendered = false;
  bool forceFlagWritten = false;
  bool forceFlagCleared = false;
  bool forceFlagWriteSucceeds = true;
  bool restarted = false;
  bool buttonsPressed = false;
  unsigned long now = 0;
  unsigned long renderHomeDurationMs = 0;
  int updateCalls = 0;
  std::time_t currentUnix = 1704110400;
  std::vector<homedeck::HomeSleepRequest> sleepRequests;

  homedeck::BootControllerDeps deps() {
    homedeck::BootControllerDeps deps{};
    deps.loadFlags = [this]() {
      return homedeck::BootFlags{configured, forceConfig};
    };
    deps.clearForceConfigOnNextBoot = [this]() {
      forceConfig = false;
      forceFlagCleared = true;
      return true;
    };
    deps.setForceConfigOnNextBoot = [this]() {
      forceFlagWritten = true;
      if (!forceFlagWriteSucceeds) {
        return false;
      }
      forceConfig = true;
      return true;
    };
    deps.startConfigPortal = [this]() { portalStarted = true; };
    deps.handleConfigPortalClient = [this]() { portalHandled = true; };
    deps.restoreSystemTimeFromRtc = []() {};
    deps.renderHome = [this]() {
      homeRendered = true;
      now += renderHomeDurationMs;
    };
    deps.updateButtons = [this]() { ++updateCalls; };
    deps.areSetupButtonsPressed = [this]() { return buttonsPressed; };
    deps.millis = [this]() { return now; };
    deps.restart = [this]() { restarted = true; };
    deps.currentTime = [this]() { return currentUnix; };
    deps.enterDeepSleep = [this](const homedeck::HomeSleepRequest& request) {
      sleepRequests.push_back(request);
    };
    return deps;
  }
};

}  // namespace

void setUp() {
  setenv("TZ", "UTC", 1);
  tzset();
}

void tearDown() {
}

void test_first_boot_enters_config_mode() {
  Fixture f{};
  homedeck::BootController controller{f.deps()};

  controller.begin();

  TEST_ASSERT_TRUE(f.portalStarted);
  TEST_ASSERT_FALSE(f.homeRendered);
  TEST_ASSERT_EQUAL(homedeck::BootMode::Config, controller.mode());
}

void test_force_config_flag_is_consumed_once() {
  Fixture f{};
  f.configured = true;
  f.forceConfig = true;
  homedeck::BootController controller{f.deps()};

  controller.begin();

  TEST_ASSERT_TRUE(f.portalStarted);
  TEST_ASSERT_TRUE(f.forceFlagCleared);
  TEST_ASSERT_FALSE(f.forceConfig);
  TEST_ASSERT_EQUAL(homedeck::BootMode::Config, controller.mode());
}

void test_configured_boot_renders_home() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};

  controller.begin();

  TEST_ASSERT_TRUE(f.homeRendered);
  TEST_ASSERT_FALSE(f.portalStarted);
  TEST_ASSERT_EQUAL(homedeck::BootMode::System, controller.mode());
}

void test_ab_held_for_five_seconds_requests_config_reboot() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.buttonsPressed = true;
  f.now = 100;
  controller.update();
  f.now = 5100;
  controller.update();

  TEST_ASSERT_TRUE(f.forceFlagWritten);
  TEST_ASSERT_TRUE(f.restarted);
}

void test_ab_release_resets_hold_timer() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.buttonsPressed = true;
  f.now = 100;
  controller.update();
  f.buttonsPressed = false;
  f.now = 2000;
  controller.update();
  f.buttonsPressed = true;
  f.now = 3200;
  controller.update();

  TEST_ASSERT_FALSE(f.forceFlagWritten);
  TEST_ASSERT_FALSE(f.restarted);
}

void test_ab_held_after_startup_window_requests_config_reboot() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.now = 6000;
  f.buttonsPressed = true;
  controller.update();
  f.now = 11000;
  controller.update();

  TEST_ASSERT_TRUE(f.forceFlagWritten);
  TEST_ASSERT_TRUE(f.restarted);
}

void test_ab_does_not_restart_when_force_config_flag_write_fails() {
  Fixture f{};
  f.configured = true;
  f.forceFlagWriteSucceeds = false;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.buttonsPressed = true;
  f.now = 100;
  controller.update();
  f.now = 5100;
  controller.update();

  TEST_ASSERT_TRUE(f.forceFlagWritten);
  TEST_ASSERT_FALSE(f.forceConfig);
  TEST_ASSERT_FALSE(f.restarted);
}

void test_system_mode_does_not_sleep_before_home_display_window() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.now = 59999;
  controller.update();

  TEST_ASSERT_EQUAL(0, static_cast<int>(f.sleepRequests.size()));
}

void test_system_mode_sleeps_to_next_midnight_after_home_display_window() {
  Fixture f{};
  f.configured = true;
  f.currentUnix = 1704110400;  // 2024-01-01 12:00:00 UTC
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.now = 60000;
  controller.update();

  TEST_ASSERT_EQUAL(1, static_cast<int>(f.sleepRequests.size()));
  TEST_ASSERT_EQUAL_UINT64(43200000000ULL, f.sleepRequests[0].timerWakeupUs);
  TEST_ASSERT_EQUAL(1, f.sleepRequests[0].wakeupGpio);
  TEST_ASSERT_TRUE(f.sleepRequests[0].wakeOnLow);
}

void test_system_mode_sleep_window_starts_after_home_render_completes() {
  Fixture f{};
  f.configured = true;
  f.renderHomeDurationMs = 5000;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.now = 64999;
  controller.update();
  TEST_ASSERT_EQUAL(0, static_cast<int>(f.sleepRequests.size()));

  f.now = 65000;
  controller.update();
  TEST_ASSERT_EQUAL(1, static_cast<int>(f.sleepRequests.size()));
}

void test_system_mode_uses_one_hour_sleep_when_time_is_not_trusted() {
  Fixture f{};
  f.configured = true;
  f.currentUnix = 1000;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.now = 60000;
  controller.update();

  TEST_ASSERT_EQUAL(1, static_cast<int>(f.sleepRequests.size()));
  TEST_ASSERT_EQUAL_UINT64(3600000000ULL, f.sleepRequests[0].timerWakeupUs);
}

void test_system_mode_requests_sleep_only_once() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.now = 60000;
  controller.update();
  f.now = 70000;
  controller.update();

  TEST_ASSERT_EQUAL(1, static_cast<int>(f.sleepRequests.size()));
}

void test_config_mode_does_not_request_home_sleep() {
  Fixture f{};
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.now = 60000;
  controller.update();

  TEST_ASSERT_EQUAL(0, static_cast<int>(f.sleepRequests.size()));
  TEST_ASSERT_TRUE(f.portalHandled);
}

void test_ab_config_reboot_takes_priority_over_sleep() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.buttonsPressed = true;
  f.now = 55000;
  controller.update();
  f.now = 60000;
  controller.update();

  TEST_ASSERT_TRUE(f.forceFlagWritten);
  TEST_ASSERT_TRUE(f.restarted);
  TEST_ASSERT_EQUAL(0, static_cast<int>(f.sleepRequests.size()));
}

void test_config_mode_update_handles_portal_client() {
  Fixture f{};
  homedeck::BootController controller{f.deps()};
  controller.begin();

  controller.update();

  TEST_ASSERT_TRUE(f.portalHandled);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_first_boot_enters_config_mode);
  RUN_TEST(test_force_config_flag_is_consumed_once);
  RUN_TEST(test_configured_boot_renders_home);
  RUN_TEST(test_ab_held_for_five_seconds_requests_config_reboot);
  RUN_TEST(test_ab_release_resets_hold_timer);
  RUN_TEST(test_ab_held_after_startup_window_requests_config_reboot);
  RUN_TEST(test_ab_does_not_restart_when_force_config_flag_write_fails);
  RUN_TEST(test_system_mode_does_not_sleep_before_home_display_window);
  RUN_TEST(test_system_mode_sleeps_to_next_midnight_after_home_display_window);
  RUN_TEST(test_system_mode_sleep_window_starts_after_home_render_completes);
  RUN_TEST(test_system_mode_uses_one_hour_sleep_when_time_is_not_trusted);
  RUN_TEST(test_system_mode_requests_sleep_only_once);
  RUN_TEST(test_config_mode_does_not_request_home_sleep);
  RUN_TEST(test_ab_config_reboot_takes_priority_over_sleep);
  RUN_TEST(test_config_mode_update_handles_portal_client);
  return UNITY_END();
}
