#include <unity.h>

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
  bool restarted = false;
  bool buttonsPressed = false;
  unsigned long now = 0;
  int updateCalls = 0;

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
      forceConfig = true;
      forceFlagWritten = true;
      return true;
    };
    deps.startConfigPortal = [this]() { portalStarted = true; };
    deps.handleConfigPortalClient = [this]() { portalHandled = true; };
    deps.restoreSystemTimeFromRtc = []() {};
    deps.renderHome = [this]() { homeRendered = true; };
    deps.updateButtons = [this]() { ++updateCalls; };
    deps.areSetupButtonsPressed = [this]() { return buttonsPressed; };
    deps.millis = [this]() { return now; };
    deps.restart = [this]() { restarted = true; };
    return deps;
  }
};

}  // namespace

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
  RUN_TEST(test_config_mode_update_handles_portal_client);
  return UNITY_END();
}
