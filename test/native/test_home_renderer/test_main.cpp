#include <unity.h>

#include <M5Unified.h>
#include <qrcode.h>

#include "home_renderer.h"

namespace {

constexpr int kCenterX = 200;
constexpr int kLogoLeft = 52;
constexpr int kLogoTop = 86;
constexpr int kLogoWidth = 297;
constexpr int kLogoHeight = 40;
constexpr int kHomeLogoTop = 280;
constexpr int kApTextCenterY = 165;
constexpr int kIpTextCenterY = 218;
constexpr int kQrLeft = 72;
constexpr int kQrTop = 258;
constexpr int kQrSize = 256;

void assertRectsInsideQrBounds() {
  TEST_ASSERT_GREATER_THAN(0, static_cast<int>(M5.Display.rects.size()));
  for (const auto& rect : M5.Display.rects) {
    TEST_ASSERT_GREATER_OR_EQUAL(kQrLeft, rect.x);
    TEST_ASSERT_GREATER_OR_EQUAL(kQrTop, rect.y);
    TEST_ASSERT_LESS_OR_EQUAL(kQrLeft + kQrSize, rect.x + rect.w);
    TEST_ASSERT_LESS_OR_EQUAL(kQrTop + kQrSize, rect.y + rect.h);
    TEST_ASSERT_EQUAL(TFT_BLACK, rect.color);
  }
}

}  // namespace

void setUp() {
  M5 = FakeM5Global{};
  gLastQrCodeText.clear();
}

void tearDown() {
}

void test_home_renderer_draws_centered_logo_in_portrait() {
  homedeck::HomeRenderer renderer;

  renderer.render();

  TEST_ASSERT_EQUAL(0, M5.Display.rotation);
  TEST_ASSERT_EQUAL(TFT_WHITE, M5.Display.fillScreenColor);
  TEST_ASSERT_EQUAL(0, static_cast<int>(M5.Display.prints.size()));
  TEST_ASSERT_EQUAL(1, static_cast<int>(M5.Display.pngDraws.size()));
  TEST_ASSERT_EQUAL(1, static_cast<int>(M5.Display.spritePushes.size()));
  TEST_ASSERT_EQUAL_STRING("/logo.png", M5.Display.pngDraws[0].path.c_str());
  TEST_ASSERT_EQUAL(kLogoLeft, M5.Display.pngDraws[0].x);
  TEST_ASSERT_EQUAL(kHomeLogoTop, M5.Display.pngDraws[0].y);
  TEST_ASSERT_EQUAL(kLogoWidth, M5.Display.pngDraws[0].maxWidth);
  TEST_ASSERT_EQUAL(kLogoHeight, M5.Display.pngDraws[0].maxHeight);
  TEST_ASSERT_EQUAL(static_cast<int>(datum_t::top_left), M5.Display.pngDraws[0].datum);
  TEST_ASSERT_EQUAL(0, M5.Display.directPngDrawCount);
  TEST_ASSERT_EQUAL(1, M5.Display.waitDisplayCount);
}

void test_home_renderer_draws_config_portal_layout() {
  homedeck::HomeRenderer renderer;

  renderer.renderConfigPortal("HomeDeck-ABCD", "192.168.4.1");

  TEST_ASSERT_EQUAL(0, M5.Display.rotation);
  TEST_ASSERT_EQUAL(TFT_WHITE, M5.Display.fillScreenColor);
  TEST_ASSERT_EQUAL(static_cast<int>(textdatum_t::middle_center), static_cast<int>(M5.Display.textDatum));
  TEST_ASSERT_EQUAL(1, static_cast<int>(M5.Display.spritePushes.size()));
  TEST_ASSERT_EQUAL(0, M5.Display.spritePushes[0].x);
  TEST_ASSERT_EQUAL(0, M5.Display.spritePushes[0].y);
  TEST_ASSERT_EQUAL(400, M5.Display.spritePushes[0].w);
  TEST_ASSERT_EQUAL(600, M5.Display.spritePushes[0].h);

  TEST_ASSERT_EQUAL(1, static_cast<int>(M5.Display.pngDraws.size()));
  TEST_ASSERT_EQUAL_STRING("/logo.png", M5.Display.pngDraws[0].path.c_str());
  TEST_ASSERT_EQUAL(kLogoLeft, M5.Display.pngDraws[0].x);
  TEST_ASSERT_EQUAL(kLogoTop, M5.Display.pngDraws[0].y);
  TEST_ASSERT_EQUAL(kLogoWidth, M5.Display.pngDraws[0].maxWidth);
  TEST_ASSERT_EQUAL(kLogoHeight, M5.Display.pngDraws[0].maxHeight);
  TEST_ASSERT_EQUAL(static_cast<int>(datum_t::top_left), M5.Display.pngDraws[0].datum);

  TEST_ASSERT_EQUAL(2, static_cast<int>(M5.Display.prints.size()));
  TEST_ASSERT_EQUAL_STRING("HomeDeck-ABCD", M5.Display.prints[0].text.c_str());
  TEST_ASSERT_EQUAL_STRING("192.168.4.1", M5.Display.prints[1].text.c_str());
  TEST_ASSERT_EQUAL(kCenterX, M5.Display.prints[0].x);
  TEST_ASSERT_EQUAL(kApTextCenterY, M5.Display.prints[0].y);
  TEST_ASSERT_EQUAL(kCenterX, M5.Display.prints[1].x);
  TEST_ASSERT_EQUAL(kIpTextCenterY, M5.Display.prints[1].y);
  TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kConfigPortal), static_cast<int>(M5.Display.prints[0].fontKind));
  TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kConfigPortal), static_cast<int>(M5.Display.prints[1].fontKind));

  TEST_ASSERT_EQUAL_STRING("WIFI:T:nopass;S:HomeDeck-ABCD;;", gLastQrCodeText.c_str());
  assertRectsInsideQrBounds();
  TEST_ASSERT_EQUAL(0, M5.Display.directRectCount);
  TEST_ASSERT_EQUAL(0, M5.Display.directPngDrawCount);
  TEST_ASSERT_EQUAL(1, M5.Display.waitDisplayCount);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_home_renderer_draws_centered_logo_in_portrait);
  RUN_TEST(test_home_renderer_draws_config_portal_layout);
  return UNITY_END();
}
