#include <unity.h>

#include <M5Unified.h>

#include "home_renderer.h"

void setUp() {
  M5 = FakeM5Global{};
}

void tearDown() {
}

void test_home_renderer_draws_centered_home_deck_in_portrait() {
  homedeck::HomeRenderer renderer;

  renderer.render();

  TEST_ASSERT_EQUAL(0, M5.Display.rotation);
  TEST_ASSERT_EQUAL(TFT_WHITE, M5.Display.fillScreenColor);
  TEST_ASSERT_EQUAL(
      static_cast<int>(textdatum_t::middle_center),
      static_cast<int>(M5.Display.textDatum));
  TEST_ASSERT_EQUAL(1, static_cast<int>(M5.Display.prints.size()));
  TEST_ASSERT_EQUAL_STRING("HomeDeck", M5.Display.prints[0].text.c_str());
  TEST_ASSERT_EQUAL(M5.Display.width() / 2, M5.Display.prints[0].x);
  TEST_ASSERT_EQUAL(M5.Display.height() / 2, M5.Display.prints[0].y);
}

void test_home_renderer_draws_config_portal_ap_and_ip() {
  homedeck::HomeRenderer renderer;

  renderer.renderConfigPortal("HomeDeck-ABCD", "192.168.4.1");

  TEST_ASSERT_EQUAL(0, M5.Display.rotation);
  TEST_ASSERT_EQUAL(TFT_WHITE, M5.Display.fillScreenColor);
  TEST_ASSERT_EQUAL(3, static_cast<int>(M5.Display.prints.size()));
  TEST_ASSERT_EQUAL_STRING("Config Mode", M5.Display.prints[0].text.c_str());
  TEST_ASSERT_EQUAL_STRING("AP: HomeDeck-ABCD", M5.Display.prints[1].text.c_str());
  TEST_ASSERT_EQUAL_STRING("IP: 192.168.4.1", M5.Display.prints[2].text.c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_home_renderer_draws_centered_home_deck_in_portrait);
  RUN_TEST(test_home_renderer_draws_config_portal_ap_and_ip);
  return UNITY_END();
}
