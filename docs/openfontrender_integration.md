# OpenFontRender 集成方案：在 PaperColor 上直接使用 TTF 字体

> 目标：无需预先将 TTF 转换为 VLW，直接把 `.ttf` 文件放入 TF（microSD）卡即可使用。

---

## 1. 现状分析

### 1.1 M5GFX 原生字体支持

`M5GFX`（及其底层 `LovyanGFX`）的 `loadFont()` 仅支持 **VLW** 格式（Processing 的字体光栅化格式）。

当前项目的字体加载流程：

```
TTF 源文件（如 MiSans）
    ↓  tools/generate_device_font.py 裁剪字符子集
    ↓  tools/font_to_vlw.cpp 转换为 VLW
    ↓  编译为 C 数组，嵌入固件
    ↓  运行时 canvas.loadFont(array) 加载
```

**痛点**：
- 每次换字体或调整字号都需要重新跑转换脚本
- 固件体积随字体增大
- 无法在运行时动态调整字号

### 1.2 为什么需要 OpenFontRender

OpenFontRender 是一个基于 FreeType 的轻量级 TTF 光栅化库，专为 ESP32 等微控制器设计。它可以直接：
- 从 SD 卡读取 `.ttf` 文件
- 从 Flash 数组加载字体
- 在运行时动态指定任意字号
- 绘制抗锯齿字体
- 直接对接 `M5GFX` / `LovyanGFX`

---

## 2. 硬件条件评估

PaperColor 的硬件完全满足 OpenFontRender 的运行要求：

| 资源 | PaperColor 配置 | OpenFontRender 需求 | 结论 |
|------|----------------|---------------------|------|
| SoC | ESP32-S3 @ 240MHz 双核 | ESP32 系列均可 | ✅ 性能充足 |
| RAM | 8MB PSRAM + 512KB SRAM | 约 30KB RAM + 4KB 栈 | ✅ 完全无压力 |
| 存储 | 16MB Flash + microSD | TTF 文件放 SD 卡 | ✅ 无需占用 Flash |
| 屏幕 | 4" E-Ink 400×600 | 任意分辨率 | ✅ E-Ink 低刷新率完美匹配 |

**E-Ink 场景的特殊优势**：
PaperColor 是墨水屏，全屏刷新约 1-2 秒。OpenFontRender 的光栅化开销（约 2-5ms/字符）在 E-Ink 场景下几乎可以忽略，真正的瓶颈始终是屏幕物理刷新。

---

## 3. 依赖引入

### 3.1 修改 platformio.ini

在 `[env:m5stack-papercolor]` 的 `lib_deps` 中添加：

```ini
lib_deps =
  M5Unified = https://github.com/m5stack/M5Unified
  M5GFX = https://github.com/m5stack/M5GFX
  M5PM1 = https://github.com/m5stack/M5PM1
  QRCode = https://github.com/ricmoo/QRCode
  sourcesimian/uICAL @ ^0.0.1
  adafruit/Adafruit NeoPixel @ ^1.12.3
  takkaO/OpenFontRender @ ^1.2   ; <-- 新增
```

> OpenFontRender 有 `library.properties`，PlatformIO 可直接从 GitHub 解析并拉取。

### 3.2 库文件说明

OpenFontRender 内部自带 FreeType 2.4.12（默认识别最稳定的版本），无需额外安装 FreeType。库结构：

```
OpenFontRender/
├── src/
│   ├── OpenFontRender.h        # 主头文件
│   ├── OpenFontRender.cpp
│   └── ofrfs/                  # 文件系统 preset
│       ├── M5Stack_SD_Preset.h     # M5Stack SD 卡预设
│       ├── M5Stack_SPIFFS_Preset.h
│       └── ...
└── tools/ttf2bin/              # TTF 转二进制数组工具
```

---

## 4. 代码集成方案

### 4.1 核心思路

保留现有的 `device_font` 模块作为抽象层，在其内部实现 **双后端**：
- **TTF 后端**（新）：通过 OpenFontRender 从 SD 卡加载 `.ttf`
- **VLW 后端**（保留）：作为 fallback，当 SD 卡不可用或 TTF 文件缺失时回退

### 4.2 文件目录约定

建议在 TF 卡中建立如下目录结构：

```
SD 卡根目录/
└── fonts/
    ├── body.ttf        # 正文字体（对应 Role::kBody）
    ├── metric.ttf      # 数值字体（对应 Role::kMetricValue）
    └── time.ttf        # 时间字体（对应 Role::kTime）
```

> 字体文件名和路径可在代码中配置，也可通过 `config_store` 支持用户自定义。

### 4.3 device_font.h 改造

```cpp
#pragma once

#include <M5Unified.h>

namespace homedeck::device_font {

enum class Role {
  kBody,
  kMetricValue,
  kTime,
};

enum class Backend {
  kAuto,      // 优先 TTF，失败回退 VLW
  kVlwOnly,   // 强制使用嵌入的 VLW
};

// 初始化 TTF 后端（在 M5.begin() 和 SD 卡就绪后调用）
bool initTtfBackend();

// 选择后端模式（默认 kAuto）
void setBackend(Backend backend);

// 应用字体到 canvas（根据当前后端自动选择）
bool apply(M5Canvas& canvas, Role role);
bool applyDefault(M5Canvas& canvas);

// 直接以指定字号绘制（仅 TTF 后端支持动态字号）
bool drawTtfString(M5Canvas& canvas, Role role, const char* text,
                   int x, int y, float fontSize, uint32_t color);

}  // namespace homedeck::device_font
```

### 4.4 device_font.cpp 改造要点

```cpp
#include "device_font.h"

#include <OpenFontRender.h>
#include <ofrfs/M5Stack_SD_Preset.h>  // OpenFontRender 的 M5 SD preset

#if !defined(UNIT_TEST)
#include "generated/device_font_vlw.h"
#endif

namespace homedeck::device_font {
namespace {

// OpenFontRender 实例（每个 Role 一个，避免频繁加载/卸载）
struct TtfFontSlot {
  OpenFontRender render;
  const char* path = nullptr;
  float lastSize = 0;
  bool loaded = false;
};

TtfFontSlot gBodyFont;
TtfFontSlot gMetricFont;
TtfFontSlot gTimeFont;

Backend gBackend = Backend::kAuto;
bool gTtfAvailable = false;

#if defined(UNIT_TEST)
const std::uint8_t kTestDeviceFontVlw[] = {14};
const std::uint8_t kTestMetricFontVlw[] = {28};
const std::uint8_t kTestTimeFontVlw[] = {42};
#endif

TtfFontSlot* slotForRole(Role role) {
  switch (role) {
    case Role::kMetricValue: return &gMetricFont;
    case Role::kTime:        return &gTimeFont;
    case Role::kBody:        return &gBodyFont;
  }
  return &gBodyFont;
}

const char* pathForRole(Role role) {
  switch (role) {
    case Role::kMetricValue: return "/fonts/metric.ttf";
    case Role::kTime:        return "/fonts/time.ttf";
    case Role::kBody:        return "/fonts/body.ttf";
  }
  return "/fonts/body.ttf";
}

bool loadVlw(M5Canvas& canvas, Role role) {
#if defined(UNIT_TEST)
  switch (role) {
    case Role::kMetricValue: return canvas.loadFont(kTestMetricFontVlw);
    case Role::kTime:        return canvas.loadFont(kTestTimeFontVlw);
    case Role::kBody:        return canvas.loadFont(kTestDeviceFontVlw);
  }
#else
  switch (role) {
    case Role::kMetricValue: return canvas.loadFont(generated::kDeviceMetricFontVlw);
    case Role::kTime:        return canvas.loadFont(generated::kDeviceTimeFontVlw);
    case Role::kBody:        return canvas.loadFont(generated::kDeviceFontVlw);
  }
#endif
  return false;
}

}  // namespace

bool initTtfBackend() {
  // 使用 OpenFontRender 的 M5Stack SD preset 初始化文件系统
  // 该 preset 内部会自动处理 SD.begin() 的检测
  // 如果项目中已自行初始化 SD，可直接用 loadFont 而不包含 preset

  gBodyFont.path   = "/fonts/body.ttf";
  gMetricFont.path = "/fonts/metric.ttf";
  gTimeFont.path   = "/fonts/time.ttf";

  // 尝试加载并验证每个字体文件
  gBodyFont.loaded   = (gBodyFont.render.loadFont(gBodyFont.path) == 0);
  gMetricFont.loaded = (gMetricFont.render.loadFont(gMetricFont.path) == 0);
  gTimeFont.loaded   = (gTimeFont.render.loadFont(gTimeFont.path) == 0);

  gTtfAvailable = gBodyFont.loaded || gMetricFont.loaded || gTimeFont.loaded;

  // 绑定到 M5.Display（OpenFontRender 需要知道如何绘制像素）
  if (gBodyFont.loaded)   gBodyFont.render.setDrawer(M5.Display);
  if (gMetricFont.loaded) gMetricFont.render.setDrawer(M5.Display);
  if (gTimeFont.loaded)   gTimeFont.render.setDrawer(M5.Display);

  return gTtfAvailable;
}

void setBackend(Backend backend) {
  gBackend = backend;
}

bool apply(M5Canvas& canvas, Role role) {
  if (gBackend == Backend::kAuto && gTtfAvailable) {
    TtfFontSlot* slot = slotForRole(role);
    if (slot && slot->loaded) {
      // TTF 模式下，OpenFontRender 不使用 canvas.setFont()，
      // 而是由 OpenFontRender 自己管理绘制。
      // 这里返回 true 表示 TTF 可用，渲染器需要切换到 OpenFontRender 绘制。
      // 实际实现中，可以用一个全局标记或修改渲染器逻辑。
      return true;
    }
  }
  // Fallback 到 VLW
  return loadVlw(canvas, role);
}

bool applyDefault(M5Canvas& canvas) {
  return apply(canvas, Role::kBody);
}

// TTF 专用绘制接口（渲染器在检测到 TTF 模式时调用此函数）
bool drawTtfString(M5Canvas& /*canvas*/, Role role, const char* text,
                   int x, int y, float fontSize, uint32_t color) {
  TtfFontSlot* slot = slotForRole(role);
  if (!slot || !slot->loaded) return false;

  if (slot->lastSize != fontSize) {
    slot->render.setFontSize(fontSize);
    slot->lastSize = fontSize;
  }
  slot->render.setFontColor(color);
  slot->render.drawString(text, x, y);
  return true;
}

}  // namespace homedeck::device_font
```

### 4.5 渲染器适配（home_renderer.cpp）

当前渲染器通过 `M5Canvas` 的 API（`setTextSize`, `drawString`, `textWidth` 等）绘制文字。引入 OpenFontRender 后，需要为 TTF 模式增加一套绘制分支。

**最小侵入方案**：在 `device_font` 模块中封装一个适配层，让 OpenFontRender 的操作对渲染器透明。

具体做法：让 `apply()` 返回 TTF 模式标志，渲染器根据标志选择绘制 API：

```cpp
// 在 home_renderer.cpp 的 drawText 中：
void drawText(M5Canvas& canvas, homedeck::device_font::Role fontRole,
              int size, int fallbackSize, int x, int y,
              const std::string& text, int maxWidth) {
  bool loaded = homedeck::device_font::apply(canvas, fontRole);

  if (homedeck::device_font::isTtfMode(fontRole)) {
    // TTF 模式：使用 OpenFontRender 绘制
    const float fontSize = static_cast<float>(size * 16);  // 或根据实际像素映射
    const uint32_t color = canvas.getTextColor();
    homedeck::device_font::drawTtfString(canvas, fontRole, text.c_str(),
                                          x, y, fontSize, color);
    return;
  }

  // VLW / 默认模式：继续使用原有 M5Canvas API
  const int resolvedSize = loaded ? size : fallbackSize;
  if (!loaded && fontRole != homedeck::device_font::Role::kBody) {
    loaded = homedeck::device_font::applyDefault(canvas);
  }
  if (!loaded) {
    canvas.setFont(nullptr);
  }
  canvas.setTextSize(resolvedSize);
  // ... 原有逻辑
}
```

> 如果希望渲染器完全无感知，也可以在 `device_font` 内部封装一个 `FontContext` 对象，同时代理 `textWidth`、`drawString` 等操作。

---

## 5. 初始化时机

OpenFontRender 的初始化需要在以下两个条件满足之后：
1. `M5.begin()` 完成（屏幕和 SD 卡硬件初始化）
2. SD 卡已挂载（如果项目中有显式 `SD.begin()`）

建议放在 `boot_controller.cpp` 的 `m5Begin` lambda 之后：

```cpp
deps.m5Begin = []() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  // ... 其他配置
  M5.begin(cfg);

  // 新增：初始化 OpenFontRender TTF 后端
  homedeck::device_font::initTtfBackend();
};
```

---

## 6. 注意事项

### 6.1 SD 卡初始化

当前项目中未见显式 `SD.begin()` 调用，M5Unified 在 `M5.begin()` 中可能已经自动初始化了 SD 卡。

如果后续发现 SD 卡未挂载，可在 `m5Begin` 中补充：

```cpp
// PaperColor 的 SD 卡 CS 引脚是 GPIO47（参见 docs/PaperColor.md）
SD.begin(GPIO_NUM_47, SPI, 40000000);
```

### 6.2 字体路径和文件名

TTF 文件需要复制到 SD 卡的 `/fonts/` 目录下。首次集成时，可以用现有的 `MiSans` 字体测试：

```bash
# 假设你已有 MiSans 的 TTF 文件
cp MiSans-Regular.ttf /Volumes/SD_CARD/fonts/body.ttf
cp MiSans-Demibold.ttf /Volumes/SD_CARD/fonts/time.ttf
cp MiSans-Medium.ttf /Volumes/SD_CARD/fonts/metric.ttf
```

### 6.3 FreeType 的 Task Watchdog

OpenFontRender 在光栅化复杂字符时可能短暂占用 CPU。如果在 FreeRTOS 环境中遇到看门狗复位：
- 增加绘制任务的栈大小（≥ 8192 bytes）
- 或在 `menuconfig` 中调整看门狗超时

对于 PaperColor 这种非实时 UI（E-Ink 刷新很慢），通常不会触发看门狗。

### 6.4 字号映射

当前 VLW 方案中字号是固定像素高度（如 24px）。TTF 使用 **点（pt）** 或 **像素（px）** 作为单位。映射关系需要根据视觉效果微调：

| VLW 字号 | 建议 TTF `setFontSize` |
|----------|------------------------|
| 16px | 16.0 |
| 24px | 24.0 |
| 36px | 36.0 |
| 48px | 48.0 |

### 6.5 抗锯齿与 E-Ink

OpenFontRender 支持灰度抗锯齿（默认 8bpp）。E-Ink 屏幕的颜色深度有限（Spectra 6 是 6 色），抗锯齿效果在 E-Ink 上可能不如 LCD 明显。可以通过调整 FreeType 的渲染模式来优化：

```cpp
render.setRenderMode(OpenFontRender::RENDER_MODE_GRAY);  // 灰度抗锯齿
render.setRenderMode(OpenFontRender::RENDER_MODE_MONO);  // 单色（最快）
```

---

## 7. 回退策略

如果 SD 卡未插入、文件损坏或 TTF 加载失败，系统必须优雅回退到嵌入的 VLW 字体。

已在 `device_font::apply()` 中内置 fallback：

```cpp
bool apply(M5Canvas& canvas, Role role) {
  if (gBackend == Backend::kAuto && gTtfAvailable) {
    // 尝试 TTF...
  }
  // 任何失败都回退到 VLW
  return loadVlw(canvas, role);
}
```

---

## 8. 性能参考

基于 OpenFontRender 官方测试数据（M5Stack Basic，240MHz ESP32）：

| 操作 | 耗时 |
|------|------|
| 从 SD 卡加载 TTF | ~227 ms |
| 从 Flash 数组加载 | ~230 ms |
| 绘制单个字符（24px） | ~2-5 ms |
| 绘制一行文字（10 个字符） | ~20-50 ms |

PaperColor 的 ESP32-S3 性能优于 ESP32（Basic），实际耗时会更短。

---

## 9. 实施检查清单

- [ ] 在 `platformio.ini` 添加 `takkaO/OpenFontRender` 依赖
- [ ] 将 TTF 字体文件放入 TF 卡的 `/fonts/` 目录
- [ ] 修改 `src/device_font.h` / `src/device_font.cpp` 支持双后端
- [ ] 在 `boot_controller.cpp` 的 `m5Begin` 中调用 `initTtfBackend()`
- [ ] 修改 `home_renderer.cpp` / `setup_renderer.cpp` 支持 TTF 绘制分支
- [ ] 编译并烧录固件
- [ ] 在真实硬件上验证：有 SD 卡时用 TTF，无 SD 卡时回退 VLW
- [ ] 对比 VLW 和 TTF 的渲染视觉效果

---

## 10. 参考资料

- [OpenFontRender GitHub](https://github.com/takkaO/OpenFontRender)
- [OpenFontRender API 文档](https://takkao.github.io/OpenFontRender/)
- [M5GFX loadFont 文档](https://docs.m5stack.com/zh_CN/api/m5gfx/loadfont)
- [PaperColor 硬件规格](./PaperColor.md)
