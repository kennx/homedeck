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
  bool calendarRendered = false;
  int calendarButtonClickCount = 0;
  bool prevMonthClicked = false;
  bool nextMonthClicked = false;
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
  std::vector<int> calendarOffsets;
  std::vector<int> almanacOffsets;

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
    deps.renderAlmanac = [this]() {
      homeRendered = true;
      now += renderHomeDurationMs;
    };
    deps.renderCalendar = [this]() { calendarRendered = true; };
    deps.renderCalendarWithOffset = [this](int offset) {
      calendarOffsets.push_back(offset);
    };
    deps.renderAlmanacWithOffset = [this](int offset) {
      almanacOffsets.push_back(offset);
    };
    deps.getCalendarButtonClickCount = [this]() { return calendarButtonClickCount; };
    deps.wasPrevMonthClicked = [this]() { return prevMonthClicked; };
    deps.wasNextMonthClicked = [this]() { return nextMonthClicked; };
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

  f.now = 299999;
  controller.update();

  TEST_ASSERT_EQUAL(0, static_cast<int>(f.sleepRequests.size()));
}

void test_system_mode_sleeps_to_next_midnight_after_home_display_window() {
  Fixture f{};
  f.configured = true;
  f.currentUnix = 1704110400;  // 2024-01-01 12:00:00 UTC
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.now = 300000;
  controller.update();

  TEST_ASSERT_EQUAL(1, static_cast<int>(f.sleepRequests.size()));
  TEST_ASSERT_EQUAL_UINT64(43200000000ULL, f.sleepRequests[0].timerWakeupUs);
  TEST_ASSERT_EQUAL(1, f.sleepRequests[0].wakeupGpio);
  TEST_ASSERT_TRUE(f.sleepRequests[0].wakeOnLow);
}

void test_system_mode_sleeps_to_local_midnight_after_manual_shanghai_time() {
  setenv("TZ", "CST-8", 1);
  tzset();
  Fixture f{};
  f.configured = true;
  f.currentUnix = 1780070220;  // 2026-05-29 23:57:00 Asia/Shanghai
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.now = 300000;
  controller.update();

  TEST_ASSERT_EQUAL(1, static_cast<int>(f.sleepRequests.size()));
  TEST_ASSERT_EQUAL_UINT64(180000000ULL, f.sleepRequests[0].timerWakeupUs);
}

void test_system_mode_sleep_window_starts_after_home_render_completes() {
  Fixture f{};
  f.configured = true;
  f.renderHomeDurationMs = 5000;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.now = 304999;
  controller.update();
  TEST_ASSERT_EQUAL(0, static_cast<int>(f.sleepRequests.size()));

  f.now = 305000;
  controller.update();
  TEST_ASSERT_EQUAL(1, static_cast<int>(f.sleepRequests.size()));
}

void test_system_mode_uses_one_hour_sleep_when_time_is_not_trusted() {
  Fixture f{};
  f.configured = true;
  f.currentUnix = 1000;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.now = 300000;
  controller.update();

  TEST_ASSERT_EQUAL(1, static_cast<int>(f.sleepRequests.size()));
  TEST_ASSERT_EQUAL_UINT64(3600000000ULL, f.sleepRequests[0].timerWakeupUs);
}

void test_system_mode_requests_sleep_only_once() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.now = 300000;
  controller.update();
  f.now = 310000;
  controller.update();

  TEST_ASSERT_EQUAL(1, static_cast<int>(f.sleepRequests.size()));
}

void test_config_mode_does_not_request_home_sleep() {
  Fixture f{};
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.now = 300000;
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
  f.now = 300000;
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

void test_single_click_switches_view() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.homeRendered = false;
  f.calendarRendered = false;
  f.calendarButtonClickCount = 1;
  controller.update();

  TEST_ASSERT_TRUE(f.calendarRendered);
  TEST_ASSERT_EQUAL(homedeck::SystemView::Calendar, controller.currentView());
}

void test_double_click_resets_to_today_in_calendar() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  // 先切换到 Calendar
  f.calendarButtonClickCount = 1;
  controller.update();
  f.calendarButtonClickCount = 0;

  // 翻页到上月
  f.prevMonthClicked = true;
  controller.update();
  f.prevMonthClicked = false;

  // 双击回本月
  f.calendarOffsets.clear();
  f.calendarButtonClickCount = 2;
  controller.update();

  TEST_ASSERT_EQUAL(1, static_cast<int>(f.calendarOffsets.size()));
  TEST_ASSERT_EQUAL(0, f.calendarOffsets[0]);
}

void test_double_click_ignored_in_almanac() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.calendarOffsets.clear();
  f.calendarButtonClickCount = 2;
  controller.update();

  TEST_ASSERT_EQUAL(0, static_cast<int>(f.calendarOffsets.size()));
}

void test_prev_month_click_in_calendar() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.calendarButtonClickCount = 1;
  controller.update();
  f.calendarButtonClickCount = 0;

  f.prevMonthClicked = true;
  controller.update();

  TEST_ASSERT_EQUAL(1, static_cast<int>(f.calendarOffsets.size()));
  TEST_ASSERT_EQUAL(-1, f.calendarOffsets[0]);
}

void test_next_month_click_in_calendar() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.calendarButtonClickCount = 1;
  controller.update();
  f.calendarButtonClickCount = 0;

  f.nextMonthClicked = true;
  controller.update();

  TEST_ASSERT_EQUAL(1, static_cast<int>(f.calendarOffsets.size()));
  TEST_ASSERT_EQUAL(1, f.calendarOffsets[0]);
}

void test_month_click_ignored_in_almanac() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.prevMonthClicked = true;
  controller.update();

  TEST_ASSERT_EQUAL(0, static_cast<int>(f.calendarOffsets.size()));
}

void test_prev_day_click_in_almanac() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  TEST_ASSERT_EQUAL(homedeck::SystemView::Almanac, controller.currentView());

  f.prevMonthClicked = true;
  controller.update();

  TEST_ASSERT_EQUAL(1, static_cast<int>(f.almanacOffsets.size()));
  TEST_ASSERT_EQUAL(-1, f.almanacOffsets[0]);
}

void test_next_day_click_in_almanac() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.nextMonthClicked = true;
  controller.update();

  TEST_ASSERT_EQUAL(1, static_cast<int>(f.almanacOffsets.size()));
  TEST_ASSERT_EQUAL(1, f.almanacOffsets[0]);
}

void test_day_click_ignored_in_calendar() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.calendarButtonClickCount = 1;
  controller.update();
  f.calendarButtonClickCount = 0;

  f.prevMonthClicked = true;
  controller.update();

  TEST_ASSERT_EQUAL(0, static_cast<int>(f.almanacOffsets.size()));
}

void test_continuous_prev_month_clicks() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.calendarButtonClickCount = 1;
  controller.update();
  f.calendarButtonClickCount = 0;

  f.prevMonthClicked = true;
  controller.update();
  f.prevMonthClicked = true;
  controller.update();

  TEST_ASSERT_EQUAL(2, static_cast<int>(f.calendarOffsets.size()));
  TEST_ASSERT_EQUAL(-1, f.calendarOffsets[0]);
  TEST_ASSERT_EQUAL(-2, f.calendarOffsets[1]);
}

void test_month_click_resets_sleep_timer() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.calendarButtonClickCount = 1;
  controller.update();
  f.calendarButtonClickCount = 0;

  f.now = 240000;
  f.prevMonthClicked = true;
  controller.update();
  f.prevMonthClicked = false;

  f.now = 480000;
  controller.update();
  TEST_ASSERT_EQUAL(0, static_cast<int>(f.sleepRequests.size()));

  f.now = 780000;
  controller.update();
  TEST_ASSERT_EQUAL(1, static_cast<int>(f.sleepRequests.size()));
}

void test_enter_system_mode_resets_month_offset() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  // 先翻页
  f.calendarButtonClickCount = 1;
  controller.update();
  f.calendarButtonClickCount = 0;
  f.prevMonthClicked = true;
  controller.update();
  f.prevMonthClicked = false;

  // 重新进入 SystemMode（模拟唤醒）
  controller.begin();
  f.calendarOffsets.clear();
  f.calendarButtonClickCount = 1;
  controller.update();
  f.calendarButtonClickCount = 0;
  f.prevMonthClicked = true;
  controller.update();

  // 如果偏移被重置，翻页后偏移应为 -1
  TEST_ASSERT_EQUAL(1, static_cast<int>(f.calendarOffsets.size()));
  TEST_ASSERT_EQUAL(-1, f.calendarOffsets[0]);
}

void test_calendar_button_click_switches_view_and_resets_sleep_timer() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  TEST_ASSERT_TRUE(f.homeRendered);
  TEST_ASSERT_FALSE(f.calendarRendered);

  f.now = 240000;
  f.calendarButtonClickCount = 1;
  controller.update();

  TEST_ASSERT_TRUE(f.calendarRendered);
  TEST_ASSERT_EQUAL(homedeck::SystemView::Calendar, controller.currentView());

  f.calendarButtonClickCount = 0;
  f.now = 480000;
  controller.update();
  TEST_ASSERT_EQUAL(0, static_cast<int>(f.sleepRequests.size()));

  f.now = 780000;
  controller.update();
  TEST_ASSERT_EQUAL(1, static_cast<int>(f.sleepRequests.size()));
}

void test_second_calendar_click_switches_back_to_almanac() {
  Fixture f{};
  f.configured = true;
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.calendarButtonClickCount = 1;
  controller.update();
  TEST_ASSERT_TRUE(f.calendarRendered);

  f.homeRendered = false;
  f.calendarButtonClickCount = 1;
  controller.update();
  TEST_ASSERT_TRUE(f.homeRendered);
  TEST_ASSERT_EQUAL(homedeck::SystemView::Almanac, controller.currentView());
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
  RUN_TEST(test_system_mode_sleeps_to_local_midnight_after_manual_shanghai_time);
  RUN_TEST(test_system_mode_sleep_window_starts_after_home_render_completes);
  RUN_TEST(test_system_mode_uses_one_hour_sleep_when_time_is_not_trusted);
  RUN_TEST(test_system_mode_requests_sleep_only_once);
  RUN_TEST(test_config_mode_does_not_request_home_sleep);
  RUN_TEST(test_ab_config_reboot_takes_priority_over_sleep);
  RUN_TEST(test_config_mode_update_handles_portal_client);
  RUN_TEST(test_single_click_switches_view);
  RUN_TEST(test_double_click_resets_to_today_in_calendar);
  RUN_TEST(test_double_click_ignored_in_almanac);
  RUN_TEST(test_prev_month_click_in_calendar);
  RUN_TEST(test_next_month_click_in_calendar);
  RUN_TEST(test_month_click_ignored_in_almanac);
  RUN_TEST(test_continuous_prev_month_clicks);
  RUN_TEST(test_month_click_resets_sleep_timer);
  RUN_TEST(test_enter_system_mode_resets_month_offset);
  RUN_TEST(test_calendar_button_click_switches_view_and_resets_sleep_timer);
  RUN_TEST(test_second_calendar_click_switches_back_to_almanac);
  return UNITY_END();
}
