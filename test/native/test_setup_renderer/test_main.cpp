#include <unity.h>

#include <string>

#include "../support/fake_arduino/M5Unified.h"

FakeM5Global M5;

#include "../../../src/device_font.cpp"
#include "../../../src/setup_renderer.cpp"

namespace {

int utf8CodePointCount(const std::string& text) {
  int count = 0;
  for (unsigned char ch : text) {
    if ((ch & 0xC0) != 0x80) {
      ++count;
    }
  }

  return count;
}

int renderedWidth(const FakePrintedText& entry) {
  int width = 0;
  for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(entry.text.c_str()); *cursor != 0; ++cursor) {
    if ((*cursor & 0xC0) != 0x80) {
      width += FakeDisplay::codePointWidthFor(entry.fontKind, *cursor) * entry.size;
    }
  }

  return width;
}

int realisticLineHeightFor(const FakePrintedText& entry) {
  if (entry.fontKind == FakeFontKind::kDeviceDefault) {
    return 20 * entry.size;
  }

  return FakeDisplay::lineHeightFor(entry.fontKind) * entry.size;
}

int bottomEdgeOf(const FakePrintedText& entry) {
  return entry.y + realisticLineHeightFor(entry);
}

void resetFakes() {
  M5 = FakeM5Global{};
}

const FakePrintedText* findPrintedText(const std::string& text) {
  for (const auto& entry : M5.Display.prints) {
    if (entry.text == text) {
      return &entry;
    }
  }

  return nullptr;
}

const FakePrintedText* findPrintedTextAtY(const std::string& text, int y) {
  for (const auto& entry : M5.Display.prints) {
    if (entry.text == text && entry.y == y) {
      return &entry;
    }
  }

  return nullptr;
}

const FakePrintedText* findLastPrintedText() {
  if (M5.Display.prints.empty()) {
    return nullptr;
  }

  return &M5.Display.prints.back();
}

const FakeRect* findLargestRect() {
  if (M5.Display.rects.empty()) {
    return nullptr;
  }

  const FakeRect* largest = &M5.Display.rects.front();
  int largestArea = largest->w * largest->h;
  for (const auto& rect : M5.Display.rects) {
    const int area = rect.w * rect.h;
    if (area > largestArea) {
      largest = &rect;
      largestArea = area;
    }
  }

  return largest;
}

}  // namespace

void test_render_uses_device_default_font_for_all_text() {
  resetFakes();

  SetupRenderer renderer;
  renderer.render("HomeDeck Setup", "192.168.4.1");

  TEST_ASSERT_FALSE(M5.Display.textWrap);

  const FakePrintedText* title = findPrintedText("HomeDeck 配网");
  TEST_ASSERT_NOT_NULL(title);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(title->fontKind));

  const FakePrintedText* step1 = findPrintedText("1. 连接开放热点");
  TEST_ASSERT_NOT_NULL(step1);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(step1->fontKind));

  const FakePrintedText* ssid = findPrintedText("HomeDeck Setup");
  TEST_ASSERT_NOT_NULL(ssid);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(ssid->fontKind));

  const FakePrintedText* step2Label = findPrintedText("2. 打开 ");
  TEST_ASSERT_NOT_NULL(step2Label);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(step2Label->fontKind));

  const FakePrintedText* step2Address = findPrintedText("192.168.4.1");
  TEST_ASSERT_NOT_NULL(step2Address);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(step2Address->fontKind));
  TEST_ASSERT_EQUAL_INT(step2Label->y, step2Address->y);
  TEST_ASSERT_EQUAL_INT(step2Label->x + renderedWidth(*step2Label), step2Address->x);

  const FakePrintedText* ipLabel = findPrintedText("当前热点 IP: ");
  TEST_ASSERT_NOT_NULL(ipLabel);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(ipLabel->fontKind));

  const FakePrintedText* ipText = &M5.Display.prints.back();
  TEST_ASSERT_NOT_NULL(ipText);
  TEST_ASSERT_EQUAL_STRING("192.168.4.1", ipText->text.c_str());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(ipText->fontKind));
}

void test_render_keeps_text_above_qr_code_region() {
  resetFakes();

  SetupRenderer renderer;
  renderer.render("HomeDeck Setup", "10.0.0.23");

  const FakePrintedText* title = findPrintedText("HomeDeck 配网");
  TEST_ASSERT_NOT_NULL(title);

  const FakePrintedText* step1 = findPrintedText("1. 连接开放热点");
  TEST_ASSERT_NOT_NULL(step1);

  const FakePrintedText* ssid = findPrintedText("HomeDeck Setup");
  TEST_ASSERT_NOT_NULL(ssid);

  const FakePrintedText* step2Label = findPrintedText("2. 打开 ");
  TEST_ASSERT_NOT_NULL(step2Label);

  const FakePrintedText* ipLabel = findPrintedText("当前热点 IP: ");
  TEST_ASSERT_NOT_NULL(ipLabel);

  const FakePrintedText* step2Address = findPrintedTextAtY("10.0.0.23", step2Label->y);
  TEST_ASSERT_NOT_NULL(step2Address);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(step2Address->fontKind));

  const FakePrintedText* lastLineLabel = findPrintedText("当前热点 IP: ");
  TEST_ASSERT_NOT_NULL(lastLineLabel);

  const FakePrintedText* lastLine = findLastPrintedText();
  TEST_ASSERT_NOT_NULL(lastLine);
  TEST_ASSERT_EQUAL_STRING("10.0.0.23", lastLine->text.c_str());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FakeFontKind::kDeviceDefault), static_cast<int>(lastLine->fontKind));

  const FakeRect* qrRegion = findLargestRect();
  TEST_ASSERT_NOT_NULL(qrRegion);

  TEST_ASSERT_TRUE(bottomEdgeOf(*title) <= step1->y);
  TEST_ASSERT_TRUE(bottomEdgeOf(*step1) <= ssid->y);
  TEST_ASSERT_TRUE(bottomEdgeOf(*ssid) <= step2Label->y);
  TEST_ASSERT_TRUE(std::max(bottomEdgeOf(*step2Label), bottomEdgeOf(*step2Address)) <= ipLabel->y);
  TEST_ASSERT_TRUE(std::max(bottomEdgeOf(*lastLineLabel), bottomEdgeOf(*lastLine)) <= qrRegion->y);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_render_uses_device_default_font_for_all_text);
  RUN_TEST(test_render_keeps_text_above_qr_code_region);
  return UNITY_END();
}
