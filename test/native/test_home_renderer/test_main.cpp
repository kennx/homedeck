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

void test_home_renderer_draws_lunar_calendar_portrait() {
  homedeck::HomeRenderer renderer;

  renderer.render();

  TEST_ASSERT_EQUAL(0, M5.Display.rotation);
  TEST_ASSERT_EQUAL(TFT_WHITE, M5.Display.fillScreenColor);
  TEST_ASSERT_EQUAL(0, static_cast<int>(M5.Display.pngDraws.size()));
  TEST_ASSERT_EQUAL(1, static_cast<int>(M5.Display.spritePushes.size()));

  // 验证年、月、周的文本及位置
  bool foundYear = false;
  bool foundMonth = false;
  bool foundWeekday = false;
  bool foundDay = false;
  bool foundLunar = false;
  bool foundGanzhi = false;

  for (const auto& print : M5.Display.prints) {
    if (print.text == "2026 年") {
      TEST_ASSERT_EQUAL(12, print.x);
      TEST_ASSERT_EQUAL(24, print.y);
      TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(print.fontKind));
      foundYear = true;
    } else if (print.text == "十二月") {
      TEST_ASSERT_EQUAL(200, print.x);
      TEST_ASSERT_EQUAL(24, print.y);
      TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(print.fontKind));
      foundMonth = true;
    } else if (print.text == "星期五") {
      TEST_ASSERT_EQUAL(388, print.x);
      TEST_ASSERT_EQUAL(24, print.y);
      TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(print.fontKind));
      foundWeekday = true;
    } else if (print.text == "21") {
      TEST_ASSERT_EQUAL(200, print.x);
      TEST_ASSERT_EQUAL(64, print.y);
      TEST_ASSERT_EQUAL(2, print.size);
      TEST_ASSERT_EQUAL(static_cast<int>(FakeFontKind::kDeviceLargeDate), static_cast<int>(print.fontKind));
      foundDay = true;
    } else if (print.text == "四月初六 小满") {
      TEST_ASSERT_EQUAL(200, print.x);
      TEST_ASSERT_EQUAL(242, print.y);
      foundLunar = true;
    } else if (print.text == "丙午年 癸巳月 丁酉日 鸡日") {
      TEST_ASSERT_EQUAL(200, print.x);
      TEST_ASSERT_EQUAL(278, print.y);
      foundGanzhi = true;
    }
  }

  TEST_ASSERT_TRUE(foundYear);
  TEST_ASSERT_TRUE(foundMonth);
  TEST_ASSERT_TRUE(foundWeekday);
  TEST_ASSERT_TRUE(foundDay);
  TEST_ASSERT_TRUE(foundLunar);
  TEST_ASSERT_TRUE(foundGanzhi);

  // 验证网格边框和线条的绘制
  bool foundTableBorder = false;
  int internalLineCount = 0;

  for (const auto& rect : M5.Display.rects) {
    if (rect.x == 12 && rect.y == 320 && rect.w == 376 && rect.h == 268) {
      foundTableBorder = true;
    } else if (rect.x == 12 && rect.w == 376 && rect.h == 1) {
      if (rect.y == 362 || rect.y == 404 || rect.y == 514) {
        internalLineCount++;
      }
    }
  }

  TEST_ASSERT_TRUE(foundTableBorder);
  TEST_ASSERT_EQUAL(3, internalLineCount);
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
  RUN_TEST(test_home_renderer_draws_lunar_calendar_portrait);
  RUN_TEST(test_home_renderer_draws_config_portal_layout);
  return UNITY_END();
}
