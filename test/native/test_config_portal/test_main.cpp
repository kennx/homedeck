#include <unity.h>

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config/config_portal.h"

namespace {

homedeck::SetupConfig defaultConfig() {
  homedeck::SetupConfig config{};
  config.timezoneIana = "Asia/Shanghai";
  config.ntpServer = "pool.ntp.org";
  return config;
}

void resetFakes() {
  WiFi = FakeWiFiClass{};
  fakeArduinoResetClock();
  gLastWebServer = nullptr;
  gLastDnsServer = nullptr;
}

}  // namespace

void setUp() {
  resetFakes();
}

void tearDown() {
}

void test_config_portal_starts_dns_wildcard_to_soft_ap_ip() {
  homedeck::ConfigPortal portal;

  portal.begin("HomeDeck-ABCD", defaultConfig(), nullptr);

  TEST_ASSERT_EQUAL(WIFI_AP_STA, WiFi.modeValue);
  TEST_ASSERT_TRUE(WiFi.softApStarted);
  TEST_ASSERT_TRUE(WiFi.softApConfigCalled);
  TEST_ASSERT_EQUAL_STRING("192.168.4.1", WiFi.apLocalIp.toString().c_str());
  TEST_ASSERT_EQUAL_STRING("192.168.4.1", WiFi.apGateway.toString().c_str());
  TEST_ASSERT_EQUAL_STRING("255.255.255.0", WiFi.apSubnet.toString().c_str());
  TEST_ASSERT_NOT_NULL(gLastDnsServer);
  TEST_ASSERT_TRUE(gLastDnsServer->started);
  TEST_ASSERT_EQUAL(53, gLastDnsServer->portValue);
  TEST_ASSERT_EQUAL_STRING("*", gLastDnsServer->domain.c_str());
  TEST_ASSERT_EQUAL_STRING("192.168.4.1", gLastDnsServer->resolvedIp.c_str());
}

void test_config_portal_handle_client_processes_dns_and_web() {
  homedeck::ConfigPortal portal;
  portal.begin("HomeDeck-ABCD", defaultConfig(), nullptr);

  portal.handleClient();

  TEST_ASSERT_NOT_NULL(gLastWebServer);
  TEST_ASSERT_EQUAL(1, gLastWebServer->handleClientCount);
  TEST_ASSERT_NOT_NULL(gLastDnsServer);
  TEST_ASSERT_EQUAL(1, gLastDnsServer->processCount);
}

void test_config_portal_serves_index_on_soft_ap_root() {
  homedeck::ConfigPortal portal;
  portal.begin("HomeDeck-ABCD", defaultConfig(), nullptr);

  TEST_ASSERT_TRUE(gLastWebServer->invoke("/"));

  TEST_ASSERT_EQUAL(200, gLastWebServer->lastStatus);
  TEST_ASSERT_EQUAL_STRING("text/html; charset=utf-8", gLastWebServer->lastType.c_str());
  TEST_ASSERT_NOT_EQUAL(-1, gLastWebServer->lastBody.find("HomeDeck-ABCD"));
}

void test_config_portal_redirects_common_probe_paths_to_index() {
  homedeck::ConfigPortal portal;
  portal.begin("HomeDeck-ABCD", defaultConfig(), nullptr);

  TEST_ASSERT_TRUE(gLastWebServer->invoke("/generate_204"));

  TEST_ASSERT_EQUAL(302, gLastWebServer->lastStatus);
  TEST_ASSERT_EQUAL_STRING("text/plain", gLastWebServer->lastType.c_str());
  TEST_ASSERT_EQUAL_STRING("http://192.168.4.1/", gLastWebServer->headers_["Location"].c_str());
}

void test_config_portal_redirects_unknown_paths_to_index() {
  homedeck::ConfigPortal portal;
  portal.begin("HomeDeck-ABCD", defaultConfig(), nullptr);

  TEST_ASSERT_TRUE(gLastWebServer->invoke("/anything"));

  TEST_ASSERT_EQUAL(302, gLastWebServer->lastStatus);
  TEST_ASSERT_EQUAL_STRING("http://192.168.4.1/", gLastWebServer->headers_["Location"].c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_config_portal_starts_dns_wildcard_to_soft_ap_ip);
  RUN_TEST(test_config_portal_handle_client_processes_dns_and_web);
  RUN_TEST(test_config_portal_serves_index_on_soft_ap_root);
  RUN_TEST(test_config_portal_redirects_common_probe_paths_to_index);
  RUN_TEST(test_config_portal_redirects_unknown_paths_to_index);
  return UNITY_END();
}
