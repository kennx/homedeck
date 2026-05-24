#include "home_renderer.h"

#include <LittleFS.h>
#include <M5Unified.h>
#include <qrcode.h>

#include <cstdint>
#include <string>
#include <vector>
#include <sstream>

#include "generated/device_font_vlw.h"

namespace homedeck {
namespace {

constexpr int kLogoTopY = 86;
constexpr int kLogoWidth = 297;
constexpr int kLogoHeight = 40;
constexpr int kTextFrameHeight = 27;
constexpr int kApTextTopY = kLogoTopY + kLogoHeight + 26;
constexpr int kApTextCenterY = kApTextTopY + kTextFrameHeight / 2;
constexpr int kIpTextTopY = kApTextTopY + kTextFrameHeight + 26;
constexpr int kIpTextCenterY = kIpTextTopY + kTextFrameHeight / 2;
constexpr int kQrLeftX = 72;
constexpr int kQrTopY = kIpTextTopY + kTextFrameHeight + 26;
constexpr int kQrSize = 256;

// 供配置界面（Config Portal）渲染 Logo 使用的方法保留
int centerX() {
  return M5.Display.width() / 2;
}

int logoLeftX() {
  return (M5.Display.width() - kLogoWidth + 1) / 2;
}

void drawLogo(M5Canvas& canvas, int top) {
  if (!LittleFS.begin()) {
    return;
  }
  canvas.drawPngFile(
      LittleFS,
      "/logo.png",
      logoLeftX(),
      top,
      kLogoWidth,
      kLogoHeight,
      0,
      0,
      1.0f,
      1.0f,
      datum_t::top_left);
  LittleFS.end();
}

void prepareScreen(M5Canvas& canvas) {
  M5.Display.setRotation(0);
  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  canvas.fillSprite(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setTextDatum(textdatum_t::middle_center);
}

void pushScreen(M5Canvas& canvas) {
  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
  M5.Display.waitDisplay();
}

void loadConfigPortalFont(M5Canvas& canvas) {
  if (!canvas.loadFont(generated::kConfigPortalFontVlw)) {
    canvas.setFont(nullptr);
  }
  canvas.setTextSize(1);
}

void drawQrCode(M5Canvas& canvas, const std::string& text, int left, int top, int size) {
  QRCode qrcode;
  std::vector<std::uint8_t> qrcodeBuffer(qrcode_getBufferSize(3));
  qrcode_initText(&qrcode, qrcodeBuffer.data(), 3, ECC_LOW, text.c_str());

  for (int y = 0; y < qrcode.size; ++y) {
    for (int x = 0; x < qrcode.size; ++x) {
      if (!qrcode_getModule(&qrcode, x, y)) {
        continue;
      }

      const int moduleLeft = left + x * size / qrcode.size;
      const int moduleTop = top + y * size / qrcode.size;
      const int moduleRight = left + (x + 1) * size / qrcode.size;
      const int moduleBottom = top + (y + 1) * size / qrcode.size;
      canvas.fillRect(
          moduleLeft,
          moduleTop,
          moduleRight - moduleLeft,
          moduleBottom - moduleTop,
          TFT_BLACK);
    }
  }
}

// -------------------------------------------------------------
// 老黄历静态数据结构与渲染细节实现
// -------------------------------------------------------------

struct LunarCalendarData {
  std::string year;        // 年份，例如 "2026 年"
  std::string month;       // 月份，例如 "十二月"
  std::string day;         // 日期，例如 "21"
  std::string weekday;     // 周几，例如 "星期五"
  bool isHoliday;          // 是否为节假日（决定周六日、节假日是否变红）
  std::string lunarDate;   // 农历日期，例如 "四月初六"
  std::string solarTerm;   // 节气，例如 "小满"
  std::string ganzhi;      // 干支和生肖，例如 "丙午年 癸巳月 丁酉日 鸡日"
  std::string wuxing;      // 五行，例如 "炉中火"
  std::string chongsha;    // 冲煞，例如 "冲猴煞北"
  std::string zhishen;     // 值神，例如 "值神白虎"
  std::string jianchu;     // 建除，例如 "建除成日"
  std::string taishen;     // 胎神，例如 "胎神厨灶炉外正南"
  std::string yi;          // 宜事情
  std::string ji;          // 忌事情
};

// 默认老黄历假数据
const LunarCalendarData kDefaultLunarData = {
  "2026 年",
  "十二月",
  "21",
  "星期五",
  false,
  "四月初六",
  "小满",
  "丙午年 癸巳月 丁酉日 鸡日",
  "五行炉中火",
  "冲猴煞北",
  "值神白虎",
  "建除成日",
  "胎神厨灶炉外正南",
  "出行 搬家 搬新房 动土 祈福 安床 祭祀 修造 拆卸 起基 出火 伐木 开光 求子",
  "结婚 交易 开业 安葬 修坟 行丧"
};

// 颜色映射 (#008A4C => RGB565: 0x0449, #F40000 => RGB565: 0xF800)
constexpr std::uint16_t kGreenColor = 0x0449;
constexpr std::uint16_t kRedColor = 0xF800;

// 自动折行绘制文本辅助函数
void drawWrappedText(M5Canvas& canvas, const std::string& text, int startX, int startY, int maxWidth, int lineHeight) {
  std::vector<std::string> words;
  std::string word;
  std::istringstream tokenStream(text);
  while (tokenStream >> word) {
    words.push_back(word);
  }

  int currentX = startX;
  int currentY = startY;

  for (size_t i = 0; i < words.size(); ++i) {
    std::string toPrint = words[i];
    if (i < words.size() - 1) {
      toPrint += " ";
    }
    
    int wordWidth = canvas.textWidth(toPrint.c_str());

    if (currentX + wordWidth > startX + maxWidth) {
      currentX = startX;
      currentY += lineHeight;
    }

    canvas.drawString(toPrint.c_str(), currentX, currentY);
    currentX += wordWidth;
  }
}

}  // namespace

void HomeRenderer::render() {
  M5Canvas canvas(&M5.Display);
  prepareScreen(canvas);

  const auto& data = kDefaultLunarData;
  const std::uint16_t themeColor = data.isHoliday ? kRedColor : kGreenColor;

  // --- 第一行：年份、月份、周几 (Y = 24px) ---
  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(themeColor, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString(data.year.c_str(), 12, 24);

    canvas.setTextDatum(textdatum_t::top_center);
    canvas.drawString(data.month.c_str(), 200, 24);

    canvas.setTextDatum(textdatum_t::top_right);
    canvas.drawString(data.weekday.c_str(), 388, 24);
    canvas.unloadFont();
  }

  // --- 第二行：大数字 156px (Y = 64px，字号 78px x 2) ---
  if (canvas.loadFont(generated::kDeviceLargeDateFontVlw)) {
    canvas.setTextColor(themeColor, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::top_center);
    canvas.setTextSize(2);
    canvas.drawString(data.day.c_str(), 200, 64);
    canvas.setTextSize(1);
    canvas.unloadFont();
  }

  // --- 第三与第四行：农历与干支 (Y = 242px, Y = 278px) ---
  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(kGreenColor, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::top_center);
    
    std::string lunarLine = data.lunarDate;
    if (!data.solarTerm.empty()) {
      lunarLine += " " + data.solarTerm;
    }
    canvas.drawString(lunarLine.c_str(), 200, 242);
    canvas.drawString(data.ganzhi.c_str(), 200, 278);

    // --- 第五行：老黄历表格 (Y: 320 to 588) ---
    // 绘制表格外边框
    canvas.drawRect(12, 320, 376, 268, kGreenColor);

    // 绘制内分割横线
    canvas.drawFastHLine(12, 362, 376, kGreenColor);
    canvas.drawFastHLine(12, 404, 376, kGreenColor);
    canvas.drawFastHLine(12, 514, 376, kGreenColor);

    // 第一子行：五行、冲煞、值神 (Y: 320 - 362)
    canvas.setTextDatum(textdatum_t::middle_left);
    canvas.drawString(data.wuxing.c_str(), 24, 341);
    
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.drawString(data.chongsha.c_str(), 200, 341);
    
    canvas.setTextDatum(textdatum_t::middle_right);
    canvas.drawString(data.zhishen.c_str(), 376, 341);

    // 第二子行：建除、胎神 (Y: 362 - 404)
    canvas.setTextDatum(textdatum_t::middle_left);
    canvas.drawString(data.jianchu.c_str(), 24, 383);
    
    canvas.setTextDatum(textdatum_t::middle_right);
    canvas.drawString(data.taishen.c_str(), 376, 383);

    // 第三子行：宜 (Y: 404 - 514，上内边距 12px)
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString("宜", 24, 416);
    drawWrappedText(canvas, data.yi, 72, 416, 304, 28);

    // 第四子行：忌 (Y: 514 - 588，上内边距 12px)
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.drawString("忌", 24, 526);
    drawWrappedText(canvas, data.ji, 72, 526, 304, 28);

    canvas.unloadFont();
  }

  pushScreen(canvas);
}

void HomeRenderer::renderConfigPortal(const std::string& apSsid, const std::string& ipAddress) {
  M5Canvas canvas(&M5.Display);
  prepareScreen(canvas);

  drawLogo(canvas, kLogoTopY);

  loadConfigPortalFont(canvas);
  canvas.drawString(apSsid.c_str(), centerX(), kApTextCenterY);
  canvas.drawString(ipAddress.c_str(), centerX(), kIpTextCenterY);
  canvas.unloadFont();

  const std::string qrText = std::string("WIFI:T:nopass;S:") + apSsid + ";;";
  drawQrCode(canvas, qrText, kQrLeftX, kQrTopY, kQrSize);
  pushScreen(canvas);
}

}  // namespace homedeck
