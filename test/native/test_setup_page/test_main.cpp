#include <unity.h>

#include "setup_page.h"

void test_select_top_five_wifi_networks_by_rssi() {
  std::vector<homedeck::WifiNetwork> networks = {
      {"weak", -90},
      {"best", -20},
      {"mid", -60},
      {"ok", -50},
      {"fine", -40},
      {"last", -70},
  };

  const auto selected = homedeck::selectTopWifiNetworks(networks, 5);

  TEST_ASSERT_EQUAL(5, static_cast<int>(selected.size()));
  TEST_ASSERT_EQUAL_STRING("best", selected[0].ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("fine", selected[1].ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("ok", selected[2].ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("mid", selected[3].ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("last", selected[4].ssid.c_str());
}

void test_setup_page_contains_wifi_list_timezone_and_disabled_auto_when_ssid_empty() {
  homedeck::SetupConfig config{};
  config.timezoneIana = "Asia/Shanghai";
  config.ntpServer = "pool.ntp.org";
  std::vector<homedeck::WifiNetwork> networks = {{"Home", -30}, {"Cafe", -50}};

  const std::string html = homedeck::buildSetupPageHtml("HomeDeck-ABCD", config, networks, "");

  TEST_ASSERT_NOT_EQUAL(-1, html.find("HomeDeck-ABCD"));
  TEST_ASSERT_NOT_EQUAL(-1, html.find("Home"));
  TEST_ASSERT_NOT_EQUAL(-1, html.find("Cafe"));
  TEST_ASSERT_NOT_EQUAL(-1, html.find("Asia/Shanghai"));
  TEST_ASSERT_NOT_EQUAL(-1, html.find("name=\"auto_rtc\""));
  TEST_ASSERT_NOT_EQUAL(-1, html.find("disabled"));
  TEST_ASSERT_NOT_EQUAL(-1, html.find("name=\"manual_datetime\""));
}

void test_setup_page_shows_error_message() {
  homedeck::SetupConfig config{};
  std::vector<homedeck::WifiNetwork> networks{};

  const std::string html = homedeck::buildSetupPageHtml("HomeDeck-ABCD", config, networks, "请填写手动时间。");

  TEST_ASSERT_NOT_EQUAL(-1, html.find("请填写手动时间。"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_select_top_five_wifi_networks_by_rssi);
  RUN_TEST(test_setup_page_contains_wifi_list_timezone_and_disabled_auto_when_ssid_empty);
  RUN_TEST(test_setup_page_shows_error_message);
  return UNITY_END();
}
