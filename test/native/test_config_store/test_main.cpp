#include <unity.h>

#include <Preferences.h>

#include "config/config_store.h"

void setUp() {
  fakePreferencesReset();
}

void tearDown() {
}

void test_load_defaults_when_empty() {
  homedeck::ConfigStore store;
  TEST_ASSERT_TRUE(store.begin());

  const auto config = store.loadSetupConfig();
  const auto flags = store.loadBootFlags();

  TEST_ASSERT_EQUAL_STRING("Asia/Shanghai", config.timezoneIana.c_str());
  TEST_ASSERT_EQUAL_STRING("pool.ntp.org", config.ntpServer.c_str());
  TEST_ASSERT_FALSE(flags.configured);
  TEST_ASSERT_FALSE(flags.forceConfigOnNextBoot);
}

void test_save_and_load_config_and_flags() {
  homedeck::ConfigStore store;
  TEST_ASSERT_TRUE(store.begin());
  homedeck::SetupConfig config{};
  config.wifiSsid = "Cafe";
  config.wifiPassword = "";
  config.timezoneIana = "Asia/Shanghai";
  config.autoRtcCorrection = true;
  config.ntpServer = "time.cloudflare.com";

  TEST_ASSERT_TRUE(store.saveSetupConfig(config));
  TEST_ASSERT_TRUE(store.saveConfigured(true));
  TEST_ASSERT_TRUE(store.setForceConfigOnNextBoot(true));

  const auto loaded = store.loadSetupConfig();
  const auto flags = store.loadBootFlags();

  TEST_ASSERT_EQUAL_STRING("Cafe", loaded.wifiSsid.c_str());
  TEST_ASSERT_EQUAL_STRING("", loaded.wifiPassword.c_str());
  TEST_ASSERT_EQUAL_STRING("Asia/Shanghai", loaded.timezoneIana.c_str());
  TEST_ASSERT_TRUE(loaded.autoRtcCorrection);
  TEST_ASSERT_EQUAL_STRING("time.cloudflare.com", loaded.ntpServer.c_str());
  TEST_ASSERT_TRUE(flags.configured);
  TEST_ASSERT_TRUE(flags.forceConfigOnNextBoot);
}

void test_clear_force_config_flag() {
  homedeck::ConfigStore store;
  TEST_ASSERT_TRUE(store.begin());
  TEST_ASSERT_TRUE(store.setForceConfigOnNextBoot(true));
  TEST_ASSERT_TRUE(store.clearForceConfigOnNextBoot());

  TEST_ASSERT_FALSE(store.loadBootFlags().forceConfigOnNextBoot);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_load_defaults_when_empty);
  RUN_TEST(test_save_and_load_config_and_flags);
  RUN_TEST(test_clear_force_config_flag);
  return UNITY_END();
}
