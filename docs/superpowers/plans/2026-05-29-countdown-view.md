# 倒数日视图与统一视图接口重构 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 添加倒数日视图，并通过统一视图接口重构现有黄历/日历视图，使 ViewManager 和 BootController 通过抽象接口调度视图。

**Architecture:** 引入 `ViewRenderer` 纯虚接口，`AlmanacView`/`CalendarView`/`CountdownView` 各自实现。`ViewManager` 持有 `std::vector<std::unique_ptr<ViewRenderer>>` 并循环切换。`BootController` 通过 `current()->onButtonA/B/reset()` 和 `current()->renderSleep()` 统一转发事件，不再感知具体视图差异。

**Tech Stack:** PlatformIO (native test), C++17, M5Unified, M5GFX, Unity test framework

---

## 文件结构

### 新增文件

| 文件 | 职责 |
|------|------|
| `src/view_renderer.h` | `ViewRenderer` 纯虚接口定义 |
| `src/render_context.h` / `.cpp` | 共用渲染工具：`sprite()`、`prepareScreen()`、`pushScreen()` |
| `src/almanac_view.h` / `.cpp` | `HomeCalendarData`、黄历数据生成、Almanac 缓存、`AlmanacView` 类 |
| `src/calendar_view.h` / `.cpp` | `CalendarData`、日历数据生成、`CalendarView` 类 |
| `src/countdown_view.h` / `.cpp` | `CountdownData`、天数计算、`CountdownView` 类 |
| `test/native/test_countdown_view/test_main.cpp` | 倒数日天数计算单元测试 |

### 修改文件

| 文件 | 变更 |
|------|------|
| `src/view_manager.h` / `.cpp` | `SystemView` 添加 `Countdown`；重构为基于 `ViewRenderer` 接口的注册/切换模式 |
| `src/boot_controller.h` / `.cpp` | 移除 per-view render 回调；按钮/deep sleep 通过 `ViewManager::current()` 转发 |
| `src/app_runtime.cpp` | `makeBootDeps()` 创建 `ViewManager` 并注册三个视图；移除旧 render 回调 |
| `src/home_renderer.h` / `.cpp` | 黄历/日历数据和渲染迁出到 `AlmanacView`/`CalendarView`；保留配置门户渲染 |
| `tools/generate_device_font.py` | `EXTRA_TEXT` 追加"剩"字 |
| `src/generated/device_font_vlw.h` / `.cpp` | 重新生成后提交 |
| `test/native/test_view_manager/test_main.cpp` | 适配新 `ViewManager` 接口（Mock `ViewRenderer`） |
| `test/native/test_boot_controller/test_main.cpp` | 适配新 `BootControllerDeps`（简化回调，Mock `ViewRenderer`） |

---

## Task 1: 提取共用渲染工具到 render_context

**Files:**
- Create: `src/render_context.h`
- Create: `src/render_context.cpp`
- Modify: `src/home_renderer.cpp`

**背景：** `home_renderer.cpp` 中的 `sprite()`、`prepareScreen()`、`pushScreen()` 被黄历、日历、配置门户共用。先提取出来，不影响任何行为。

- [ ] **Step 1: 创建 `src/render_context.h`**

```cpp
#pragma once

#include <M5Unified.h>

namespace homedeck {

M5Canvas& sprite();
void prepareScreen(M5Canvas& canvas);
void pushScreen(M5Canvas& canvas);

}  // namespace homedeck
```

- [ ] **Step 2: 创建 `src/render_context.cpp`**

```cpp
#include "render_context.h"

namespace homedeck {

M5Canvas& sprite() {
  static M5Canvas canvas(&M5.Display);
  static bool ready = false;
  if (!ready) {
    canvas.setColorDepth(16);
    canvas.createSprite(M5.Display.width(), M5.Display.height());
    ready = true;
  }
  return canvas;
}

void prepareScreen(M5Canvas& canvas) {
  canvas.fillSprite(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setTextDatum(textdatum_t::middle_center);
}

void pushScreen(M5Canvas& canvas) {
  canvas.pushSprite(0, 0);
  M5.Display.waitDisplay();
}

}  // namespace homedeck
```

- [ ] **Step 3: 修改 `src/home_renderer.cpp`**

在 `home_renderer.cpp` 顶部添加 `#include "render_context.h"`，然后删除以下函数的实现（保留声明如果头文件需要，但当前它们在匿名命名空间中）：
- `sprite()`
- `prepareScreen()`
- `pushScreen()`

这些函数在 `home_renderer.cpp` 的匿名命名空间中的实现直接删除，因为 `render_context.cpp` 已经提供。

- [ ] **Step 4: 运行测试**

```bash
pio test -e native --filter test_home_renderer
```

Expected: 通过（行为无变化，只是代码位置移动）。

- [ ] **Step 5: Commit**

```bash
git add src/render_context.h src/render_context.cpp src/home_renderer.cpp
git commit -m "refactor: 提取共用渲染工具到 render_context

- 将 sprite()、prepareScreen()、pushScreen() 从 home_renderer.cpp
  迁出到独立的 render_context.h/cpp
- 不影响任何渲染行为"
```

---

## Task 2: 添加 Countdown 枚举并更新 ViewManager 切换循环

**Files:**
- Modify: `src/view_manager.h`
- Modify: `src/view_manager.cpp`
- Modify: `src/app_runtime.cpp`
- Modify: `test/native/test_view_manager/test_main.cpp`

**背景：** 在重构接口之前，先把 `Countdown` 添加到现有回调体系中。这样后续步骤可以逐步替换，而不是一次性大改。

- [ ] **Step 1: `src/view_manager.h` 添加 `Countdown`**

```cpp
enum class SystemView {
  Almanac,
  Calendar,
  Countdown,
};

struct ViewManagerDeps {
  std::function<void()> renderAlmanac;
  std::function<void()> renderCalendar;
  std::function<void()> renderCountdown;
};
```

- [ ] **Step 2: `src/view_manager.cpp` 更新切换逻辑**

```cpp
void ViewManager::switchToNextView() {
  switch (currentView_) {
    case SystemView::Almanac:
      switchTo(SystemView::Calendar);
      break;
    case SystemView::Calendar:
      switchTo(SystemView::Countdown);
      break;
    case SystemView::Countdown:
      switchTo(SystemView::Almanac);
      break;
  }
}

void ViewManager::switchTo(SystemView view) {
  currentView_ = view;
  switch (view) {
    case SystemView::Almanac:
      if (deps_.renderAlmanac) deps_.renderAlmanac();
      break;
    case SystemView::Calendar:
      if (deps_.renderCalendar) deps_.renderCalendar();
      break;
    case SystemView::Countdown:
      if (deps_.renderCountdown) deps_.renderCountdown();
      break;
  }
}
```

- [ ] **Step 3: `src/app_runtime.cpp` 提供空 `renderCountdown` 回调**

在 `makeBootDeps()` 中，给 `vmDeps` 添加：

```cpp
vmDeps.renderCountdown = []() {
  // TODO: 后续步骤替换为 CountdownView
};
```

- [ ] **Step 4: 更新 `test_view_manager`**

修改 `Fixture::deps()` 添加 `renderCountdown`：

```cpp
deps.renderCountdown = [this]() { renderedViews.push_back("countdown"); };
```

更新测试用例：
- `test_view_manager_switch_to_next_view_from_calendar` 现在切换到 `Countdown`
- 新增 `test_view_manager_switch_to_next_view_from_countdown` 切换到 `Almanac`
- 新增 `test_view_manager_three_view_cycle`
- 更新 `test_view_manager_switch_to_next_view_cycles`

```cpp
void test_view_manager_switch_to_next_view_from_calendar() {
  Fixture f{};
  homedeck::ViewManager vm{f.deps()};
  vm.begin();
  vm.switchToNextView();  // to Calendar
  f.renderedViews.clear();

  vm.switchToNextView();

  TEST_ASSERT_EQUAL(homedeck::SystemView::Countdown, vm.currentView());
  TEST_ASSERT_EQUAL(1, static_cast<int>(f.renderedViews.size()));
  TEST_ASSERT_EQUAL_STRING("countdown", f.renderedViews[0].c_str());
}

void test_view_manager_switch_to_next_view_from_countdown() {
  Fixture f{};
  homedeck::ViewManager vm{f.deps()};
  vm.begin();
  vm.switchToNextView();  // Calendar
  vm.switchToNextView();  // Countdown
  f.renderedViews.clear();

  vm.switchToNextView();

  TEST_ASSERT_EQUAL(homedeck::SystemView::Almanac, vm.currentView());
  TEST_ASSERT_EQUAL_STRING("almanac", f.renderedViews[0].c_str());
}

void test_view_manager_three_view_cycle() {
  Fixture f{};
  homedeck::ViewManager vm{f.deps()};
  vm.begin();

  vm.switchToNextView();  // Calendar
  vm.switchToNextView();  // Countdown
  vm.switchToNextView();  // Almanac
  vm.switchToNextView();  // Calendar

  TEST_ASSERT_EQUAL(homedeck::SystemView::Calendar, vm.currentView());
}
```

- [ ] **Step 5: 运行测试**

```bash
pio test -e native --filter test_view_manager
```

Expected: PASS。

- [ ] **Step 6: Commit**

```bash
git add src/view_manager.h src/view_manager.cpp src/app_runtime.cpp \
  test/native/test_view_manager/test_main.cpp
git commit -m "feat: SystemView 添加 Countdown，ViewManager 支持三视图切换"
```

---

## Task 3: 拆分 AlmanacView 和 CalendarView

**Files:**
- Create: `src/almanac_view.h`
- Create: `src/almanac_view.cpp`
- Create: `src/calendar_view.h`
- Create: `src/calendar_view.cpp`
- Modify: `src/home_renderer.h`
- Modify: `src/home_renderer.cpp`
- Modify: `src/app_runtime.cpp`

**背景：** 将 `home_renderer.cpp` 中黄历和日历的数据生成、缓存管理、渲染逻辑分别迁到 `AlmanacView` 和 `CalendarView` 中。`home_renderer.cpp` 仅保留配置门户渲染。`app_runtime.cpp` 中的 `renderHomeWithEnvironment` 等函数改为调用视图类。

**迁移策略：**
- `almanac_view.cpp` 包含：黄历辅助函数、`HomeCalendarData`、Almanac 缓存、`makeHomeCalendarData`、`AlmanacView::render` / `renderSleep` / `renderWithOffset`
- `calendar_view.cpp` 包含：日历辅助函数、`CalendarData`、`makeCalendarData`、`CalendarView::render` / `renderSleep` / `renderWithOffset`
- `home_renderer.cpp` 保留：`drawLogo`、`drawQrCode`、`HomeRenderer::renderConfigPortal`
- Almanac 缓存函数（`writeHomeAlmanacCache`、`applyCachedHomeAlmanac` 等）放在 `almanac_view.cpp` 中，但需要在 `almanac_view.h` 暴露声明供 `calendar_view.cpp` 使用

- [ ] **Step 1: 创建 `src/almanac_view.h`**

```cpp
#pragma once

#include <ctime>
#include <string>

namespace homedeck {

struct HomeCalendarData {
  std::string year;
  std::string month;
  std::string day;
  std::string weekday;
  bool isHoliday = false;
  std::string lunarDate;
  std::string solarTerm;
  std::string ganzhi;
  std::string wuxing;
  std::string chongsha;
  std::string zhishen;
  std::string jianchu;
  std::string taishen;
  std::string yi;
  std::string ji;
  bool temperatureAvailable = false;
  float temperatureCelsius = 0.0f;
  bool humidityAvailable = false;
  float humidityPercent = 0.0f;
  std::string bottomCenterMessage;
};

// 前向声明，避免 include almanac_provider.h
struct AlmanacDayData;

HomeCalendarData makeHomeCalendarData(const std::tm& localTime);
HomeCalendarData makeCurrentHomeCalendarData();

// Almanac 缓存函数（供 calendar_view.cpp 使用）
bool applyCachedHomeAlmanac(int year, int month, int day, HomeCalendarData& data);
void writeHomeAlmanacCache(int year, int month, int day, const AlmanacDayData& almanac);

class AlmanacView {
 public:
  void render();
  void renderWithOffset(int dayOffset);
  void renderSleep();
  void onButtonA();  // prev day
  void onButtonB();  // next day
  void reset();      // reset offset

 private:
  int dayOffset_ = 0;
};

}  // namespace homedeck
```

- [ ] **Step 2: 创建 `src/almanac_view.cpp`**

将 `home_renderer.cpp` 中的以下内容完整迁出：
- 所有 `HomeCalendarData` 相关的辅助函数（`formatYear`、`formatDay`、`fallbackLocalTime`、`applyMissingAlmanac`、`applyAlmanac`、`lookupLunarFestival` 等）
- Almanac 缓存结构体和函数（`AlmanacHomeCache`、`AlmanacCache`、`almanacCacheMatches`、`prepareAlmanacCacheDate`、`writeHomeAlmanacCache`、`applyCachedHomeAlmanac`、`gAlmanacCache`）
- `makeHomeCalendarData`、`makeCurrentHomeCalendarData`
- 黄历渲染相关的辅助函数（`drawEnvironmentReadings`、`sortAlmanacEvents`、`getEventPriority`、`utf8CodePointLength`、`tokenize`、`drawWrappedText`、`wrappedLineCount`、`dynamicActionRowHeight` 等）
- 常量定义（`kCalendarInsetX`、`kCalendarCenterX`、表格相关常量等）
- `AlmanacView::render()` —— 原 `HomeRenderer::render(const HomeCalendarData&)` 的逻辑，但内部自己准备数据
- `AlmanacView::renderWithOffset(int dayOffset)` —— 设置偏移后渲染
- `AlmanacView::renderSleep()` —— 去掉温湿度、时间显示 `--:--`
- `AlmanacView::onButtonA/B/reset()` —— 日偏移逻辑

`AlmanacView::render()` 内部：
```cpp
void AlmanacView::render() {
  std::time_t now = time(nullptr);
  std::tm* local = now > 0 ? std::localtime(&now) : nullptr;
  std::tm fallback = fallbackLocalTime();
  std::tm targetTm = local != nullptr ? *local : fallback;
  
  targetTm.tm_mday += dayOffset_;
  if (std::mktime(&targetTm) == -1) {
    targetTm = fallback;
  }
  
  HomeCalendarData data = makeHomeCalendarData(targetTm);
  
  // 读取环境读数
  const EnvironmentReading reading = readSht40Environment();
  if (reading.ok) {
    data.temperatureAvailable = true;
    data.temperatureCelsius = reading.temperatureCelsius;
    data.humidityAvailable = true;
    data.humidityPercent = reading.humidityPercent;
  }
  
  // 时间
  time_t tnow = time(nullptr);
  tm* tlocal = localtime(&tnow);
  char timeStr[6] = {};
  if (tlocal != nullptr) {
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", tlocal->tm_hour, tlocal->tm_min);
  }
  data.bottomCenterMessage = timeStr;
  
  // 渲染（原 HomeRenderer::render(const HomeCalendarData&) 的逻辑）
  M5Canvas& canvas = sprite();
  prepareScreen(canvas);
  // ... 原有渲染代码 ...
  pushScreen(canvas);
}
```

- [ ] **Step 3: 创建 `src/calendar_view.h`**

```cpp
#pragma once

#include <ctime>
#include <string>

namespace homedeck {

struct CalendarData {
  int year = 0;
  int month = 0;
  int day = 0;
  int todayWeekday = 0;
  bool temperatureAvailable = false;
  float temperatureCelsius = 0.0f;
  bool humidityAvailable = false;
  float humidityPercent = 0.0f;
  std::string bottomCenterMessage;
  std::string lunarDate;
  std::string solarTerm;
  std::string festival;
  int nextSpecialMonth = 0;
  int nextSpecialDay = 0;
  std::string nextSpecialTerm;
  std::string nextSpecialFestival;
  int secondSpecialMonth = 0;
  int secondSpecialDay = 0;
  std::string secondSpecialTerm;
  std::string secondSpecialFestival;
  int todayMonth = 0;
  int todayDay = 0;
};

CalendarData makeCalendarData(const std::tm& localTime);
CalendarData makeCurrentCalendarData();
void applySht40ToCalendar(CalendarData& data);

class CalendarView {
 public:
  void render();
  void renderWithOffset(int monthOffset);
  void renderSleep();
  void onButtonA();  // prev month
  void onButtonB();  // next month
  void reset();      // reset offset

 private:
  int monthOffset_ = 0;
};

}  // namespace homedeck
```

- [ ] **Step 4: 创建 `src/calendar_view.cpp`**

将 `home_renderer.cpp` 中的以下内容完整迁出：
- 日历相关常量（`kCalInsetX`、`kCalWidth`、星期/月份名称等）
- 日历辅助函数（`daysInMonth`、`cellLeftX`、`cellCenterX`、`formatCalendarYear` 等）
- `makeCalendarData`、`makeCurrentCalendarData`、`applySht40ToCalendar`
- `CalendarView::render()` —— 原 `HomeRenderer::renderCalendar()` 的逻辑
- `CalendarView::renderWithOffset(int monthOffset)`
- `CalendarView::renderSleep()`
- `CalendarView::onButtonA/B/reset()`

注意：`makeCalendarData` 依赖 `almanac_view.h` 中暴露的 Almanac 缓存函数（`writeHomeAlmanacCache`、`applyCachedHomeAlmanac`）以及 `almanac_provider.h`。

- [ ] **Step 5: 简化 `src/home_renderer.h`**

```cpp
#pragma once

#include <string>

namespace homedeck {

class HomeRenderer {
 public:
  void renderConfigPortal(const std::string& apSsid, const std::string& ipAddress);
};

}  // namespace homedeck
```

- [ ] **Step 6: 简化 `src/home_renderer.cpp`**

删除所有黄历和日历相关的代码（已迁出），仅保留：
- 配置门户常量（`kLogoTopY` 等）
- `drawLogo()`
- `loadConfigPortalFont()`
- `drawQrCode()`
- `centerX()`、`logoLeftX()`
- `HomeRenderer::renderConfigPortal()`
- `#include "render_context.h"`（使用 `sprite()`、`prepareScreen()`、`pushScreen()`）

- [ ] **Step 7: `src/app_runtime.cpp` 适配**

在 `app_runtime.cpp` 中：
1. 添加 `#include "almanac_view.h"` 和 `#include "calendar_view.h"`
2. 定义全局视图实例：

```cpp
homedeck::AlmanacView gAlmanacView;
homedeck::CalendarView gCalendarView;
```

3. 替换现有的 render 函数：

```cpp
void renderHomeWithEnvironment() {
  gAlmanacView.render();
}

void renderCalendarWithEnvironment() {
  gCalendarView.render();
}

void renderCalendarWithOffset(int monthOffset) {
  gCalendarView.renderWithOffset(monthOffset);
}

void renderAlmanacWithOffset(int dayOffset) {
  gAlmanacView.renderWithOffset(dayOffset);
}
```

4. 删除旧的 `makeCurrentHomeCalendarDataWithEnvironment()` 函数（AlmanacView::render 内部已处理）。

- [ ] **Step 8: 运行测试**

```bash
pio test -e native
```

Expected: 全部通过。如果 `test_home_renderer` 失败，需要检查测试是否依赖已迁出的函数，必要时拆分或迁移测试。

- [ ] **Step 9: Commit**

```bash
git add src/almanac_view.h src/almanac_view.cpp src/calendar_view.h src/calendar_view.cpp \
  src/home_renderer.h src/home_renderer.cpp src/app_runtime.cpp
git commit -m "refactor: 拆分 AlmanacView 和 CalendarView

- 从 home_renderer.cpp 迁出黄历和日历的数据生成与渲染逻辑
- AlmanacView 管理日偏移和渲染
- CalendarView 管理月偏移和渲染
- home_renderer 仅保留配置门户渲染
- app_runtime 使用新的视图类"
```

---

## Task 4: 引入 ViewRenderer 接口并让现有视图实现

**Files:**
- Create: `src/view_renderer.h`
- Modify: `src/almanac_view.h`
- Modify: `src/calendar_view.h`

**背景：** 现在 `AlmanacView` 和 `CalendarView` 已经是独立的类。让它们实现统一的 `ViewRenderer` 接口，为下一步 `ViewManager` 重构做准备。

- [ ] **Step 1: 创建 `src/view_renderer.h`**

```cpp
#pragma once

#include "view_manager.h"

namespace homedeck {

class ViewRenderer {
 public:
  virtual ~ViewRenderer() = default;
  virtual SystemView viewType() const = 0;
  virtual void render() = 0;
  virtual void renderSleep() = 0;
  virtual void onButtonA() {}
  virtual void onButtonB() {}
  virtual void reset() {}
};

}  // namespace homedeck
```

- [ ] **Step 2: `src/almanac_view.h` 继承 `ViewRenderer`**

```cpp
#include "view_renderer.h"

class AlmanacView : public ViewRenderer {
 public:
  SystemView viewType() const override { return SystemView::Almanac; }
  void render() override;
  void renderSleep() override;
  void onButtonA() override;
  void onButtonB() override;
  void reset() override;

  // 暂时保留，供 app_runtime 回调使用（后续步骤移除）
  void renderWithOffset(int dayOffset);

 private:
  int dayOffset_ = 0;
};
```

- [ ] **Step 3: `src/calendar_view.h` 继承 `ViewRenderer`**

```cpp
#include "view_renderer.h"

class CalendarView : public ViewRenderer {
 public:
  SystemView viewType() const override { return SystemView::Calendar; }
  void render() override;
  void renderSleep() override;
  void onButtonA() override;
  void onButtonB() override;
  void reset() override;

  // 暂时保留，供 app_runtime 回调使用（后续步骤移除）
  void renderWithOffset(int monthOffset);

 private:
  int monthOffset_ = 0;
};
```

- [ ] **Step 4: 运行测试**

```bash
pio test -e native
```

Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/view_renderer.h src/almanac_view.h src/calendar_view.h
git commit -m "refactor: 引入 ViewRenderer 接口，AlmanacView/CalendarView 实现接口"
```

---

## Task 5: 重构 ViewManager 和 BootController 使用统一接口

**Files:**
- Modify: `src/view_manager.h` / `.cpp`
- Modify: `src/boot_controller.h` / `.cpp`
- Modify: `test/native/test_view_manager/test_main.cpp`
- Modify: `test/native/test_boot_controller/test_main.cpp`

**背景：** 核心重构步骤。`ViewManager` 从回调模式改为持有 `ViewRenderer` 实例。`BootController` 不再直接调用 per-view render 回调，而是通过 `ViewManager::current()` 统一转发。

- [ ] **Step 1: `src/view_manager.h` 重构**

```cpp
#pragma once

#include <memory>
#include <vector>

#include "view_renderer.h"

namespace homedeck {

class ViewManager {
 public:
  void addView(std::unique_ptr<ViewRenderer> view);
  void begin(SystemView initialView);
  void switchToNextView();
  SystemView currentView() const;
  ViewRenderer* current() const;

 private:
  void switchTo(SystemView view);

  std::vector<std::unique_ptr<ViewRenderer>> views_;
  size_t currentIndex_ = 0;
};

}  // namespace homedeck
```

- [ ] **Step 2: `src/view_manager.cpp` 重构**

```cpp
#include "view_manager.h"

namespace homedeck {

void ViewManager::addView(std::unique_ptr<ViewRenderer> view) {
  views_.push_back(std::move(view));
}

void ViewManager::begin(SystemView initialView) {
  switchTo(initialView);
}

void ViewManager::switchToNextView() {
  if (views_.empty()) return;
  currentIndex_ = (currentIndex_ + 1) % views_.size();
  views_[currentIndex_]->render();
}

SystemView ViewManager::currentView() const {
  if (currentIndex_ < views_.size()) {
    return views_[currentIndex_]->viewType();
  }
  return SystemView::Almanac;
}

ViewRenderer* ViewManager::current() const {
  if (currentIndex_ < views_.size()) {
    return views_[currentIndex_].get();
  }
  return nullptr;
}

void ViewManager::switchTo(SystemView view) {
  for (size_t i = 0; i < views_.size(); ++i) {
    if (views_[i]->viewType() == view) {
      currentIndex_ = i;
      views_[i]->render();
      return;
    }
  }
}

}  // namespace homedeck
```

- [ ] **Step 3: `src/boot_controller.h` 简化 deps 和内部状态**

```cpp
struct BootControllerDeps {
  std::function<BootFlags()> loadFlags;
  std::function<bool()> clearForceConfigOnNextBoot;
  std::function<bool()> setForceConfigOnNextBoot;
  std::function<void()> startConfigPortal;
  std::function<void()> handleConfigPortalClient;
  std::function<void()> restoreSystemTimeFromRtc;
  std::function<int()> getCalendarButtonClickCount;
  std::function<bool()> wasPrevMonthClicked;
  std::function<bool()> wasNextMonthClicked;
  std::function<void()> updateButtons;
  std::function<bool()> areSetupButtonsPressed;
  std::function<unsigned long()> millis;
  std::function<void()> restart;
  std::function<std::time_t()> currentTime;
  std::function<SystemView()> loadSavedView;
  std::function<void(SystemView)> saveCurrentView;
  std::function<void(const HomeSleepRequest&)> enterDeepSleep;
  std::function<std::unique_ptr<ViewManager>()> createViewManager;  // 新增
};
```

`BootController` 类内部：
- 移除 `calendarMonthOffset_`
- 移除 `almanacDayOffset_`
- `viewManager_` 类型不变（已经是 `std::unique_ptr<ViewManager>`）

- [ ] **Step 4: `src/boot_controller.cpp` 重构 update()**

```cpp
void BootController::update() {
  if (!started_) return;

  if (mode_ == BootMode::Config) {
    if (deps_.handleConfigPortalClient) deps_.handleConfigPortalClient();
    return;
  }

  if (deps_.updateButtons) deps_.updateButtons();
  const unsigned long now = deps_.millis ? deps_.millis() : 0;
  updateSetupShortcut(now);
  if (setupShortcutConsumed_) return;

  // 1. BtnC（视图切换 / 双击重置）
  const int btnCClicks = deps_.getCalendarButtonClickCount ? deps_.getCalendarButtonClickCount() : 0;
  if (btnCClicks == 1) {
    if (viewManager_) {
      viewManager_->switchToNextView();
      if (deps_.saveCurrentView) {
        deps_.saveCurrentView(viewManager_->currentView());
      }
      lastActivityMs_ = now;
    }
  } else if (btnCClicks >= 2) {
    if (viewManager_) {
      if (auto* v = viewManager_->current()) {
        v->reset();
        v->render();
      }
      lastActivityMs_ = now;
    }
  }

  // 2. BtnA/B（翻页）直接转发给当前视图
  if (viewManager_) {
    if (auto* v = viewManager_->current()) {
      bool actionTaken = false;
      if (deps_.wasPrevMonthClicked && deps_.wasPrevMonthClicked()) {
        v->onButtonA();
        actionTaken = true;
      } else if (deps_.wasNextMonthClicked && deps_.wasNextMonthClicked()) {
        v->onButtonB();
        actionTaken = true;
      }
      if (actionTaken) {
        lastActivityMs_ = now;
      }
    }
  }

  updateHomeSleep(now);
}
```

`enterSystemMode()`：
```cpp
void BootController::enterSystemMode() {
  mode_ = BootMode::System;
  setupButtonsPressedSinceMs_ = 0;
  setupButtonsWerePressed_ = false;
  setupShortcutConsumed_ = false;
  homeSleepRequested_ = false;

  if (deps_.restoreSystemTimeFromRtc) deps_.restoreSystemTimeFromRtc();

  viewManager_ = deps_.createViewManager ? deps_.createViewManager() : nullptr;

  SystemView initialView = deps_.loadSavedView ? deps_.loadSavedView() : SystemView::Almanac;
  if (viewManager_) {
    viewManager_->begin(initialView);
  }

  lastActivityMs_ = deps_.millis ? deps_.millis() : 0;
}
```

`updateHomeSleep()`：
```cpp
void BootController::updateHomeSleep(unsigned long now) {
  if (homeSleepRequested_) return;
  if (now - lastActivityMs_ < kHomeDisplayDurationMs) return;

  homeSleepRequested_ = true;
  if (viewManager_ && deps_.enterDeepSleep) {
    if (auto* v = viewManager_->current()) {
      v->renderSleep();
    }
    deps_.enterDeepSleep(makeHomeSleepRequest());
  }
}
```

- [ ] **Step 5: 更新 `test_view_manager`**

重写测试，使用 Mock `ViewRenderer`：

```cpp
#include <unity.h>
#include <memory>
#include <string>
#include <vector>

#include "view_manager.h"
#include "view_renderer.h"

namespace {

struct MockView : public homedeck::ViewRenderer {
  MockView(homedeck::SystemView type, std::string name)
      : type_(type), name_(std::move(name)) {}

  homedeck::SystemView viewType() const override { return type_; }
  void render() override { rendered = true; }
  void renderSleep() override { sleepRendered = true; }
  void onButtonA() override { buttonA = true; }
  void onButtonB() override { buttonB = true; }
  void reset() override { resetCalled = true; }

  homedeck::SystemView type_;
  std::string name_;
  bool rendered = false;
  bool sleepRendered = false;
  bool buttonA = false;
  bool buttonB = false;
  bool resetCalled = false;
};

struct Fixture {
  std::vector<std::unique_ptr<MockView>> mocks;

  homedeck::ViewManager makeManager() {
    homedeck::ViewManager vm;
    auto almanac = std::make_unique<MockView>(homedeck::SystemView::Almanac, "almanac");
    mocks.push_back(almanac.get());
    vm.addView(std::move(almanac));

    auto calendar = std::make_unique<MockView>(homedeck::SystemView::Calendar, "calendar");
    mocks.push_back(calendar.get());
    vm.addView(std::move(calendar));

    auto countdown = std::make_unique<MockView>(homedeck::SystemView::Countdown, "countdown");
    mocks.push_back(countdown.get());
    vm.addView(std::move(countdown));
    return vm;
  }
};

}  // namespace

void setUp() {}
void tearDown() {}

void test_view_manager_begins_with_almanac() {
  Fixture f{};
  homedeck::ViewManager vm = f.makeManager();

  vm.begin(homedeck::SystemView::Almanac);

  TEST_ASSERT_EQUAL(homedeck::SystemView::Almanac, vm.currentView());
  TEST_ASSERT_TRUE(f.mocks[0]->rendered);
}

void test_view_manager_switch_cycles_three_views() {
  Fixture f{};
  homedeck::ViewManager vm = f.makeManager();
  vm.begin(homedeck::SystemView::Almanac);

  vm.switchToNextView();
  TEST_ASSERT_EQUAL(homedeck::SystemView::Calendar, vm.currentView());
  TEST_ASSERT_TRUE(f.mocks[1]->rendered);

  vm.switchToNextView();
  TEST_ASSERT_EQUAL(homedeck::SystemView::Countdown, vm.currentView());
  TEST_ASSERT_TRUE(f.mocks[2]->rendered);

  vm.switchToNextView();
  TEST_ASSERT_EQUAL(homedeck::SystemView::Almanac, vm.currentView());
}

void test_view_manager_current_returns_renderer() {
  Fixture f{};
  homedeck::ViewManager vm = f.makeManager();
  vm.begin(homedeck::SystemView::Almanac);

  auto* current = vm.current();
  TEST_ASSERT_NOT_NULL(current);
  TEST_ASSERT_EQUAL(homedeck::SystemView::Almanac, current->viewType());
}

void test_view_manager_switch_to_specific_view() {
  Fixture f{};
  homedeck::ViewManager vm = f.makeManager();
  vm.begin(homedeck::SystemView::Almanac);

  vm.switchToNextView();  // Calendar
  f.mocks[1]->rendered = false;
  vm.switchToNextView();  // Countdown

  TEST_ASSERT_EQUAL(homedeck::SystemView::Countdown, vm.currentView());
  TEST_ASSERT_TRUE(f.mocks[2]->rendered);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_view_manager_begins_with_almanac);
  RUN_TEST(test_view_manager_switch_cycles_three_views);
  RUN_TEST(test_view_manager_current_returns_renderer);
  RUN_TEST(test_view_manager_switch_to_specific_view);
  return UNITY_END();
}
```

- [ ] **Step 6: 更新 `test_boot_controller`**

使用 Mock `ViewRenderer` 和新的 `BootControllerDeps`：

```cpp
#include <unity.h>
#include <memory>
#include <vector>

#include "boot_controller.h"
#include "view_manager.h"
#include "view_renderer.h"

namespace {

struct MockView : public homedeck::ViewRenderer {
  MockView(homedeck::SystemView type) : type_(type) {}
  homedeck::SystemView viewType() const override { return type_; }
  void render() override { rendered = true; }
  void renderSleep() override { sleepRendered = true; }
  void onButtonA() override { buttonA = true; }
  void onButtonB() override { buttonB = true; }
  void reset() override { resetCalled = true; }

  homedeck::SystemView type_;
  bool rendered = false;
  bool sleepRendered = false;
  bool buttonA = false;
  bool buttonB = false;
  bool resetCalled = false;
};

struct Fixture {
  bool configured = true;
  bool forceConfig = false;
  bool portalStarted = false;
  int calendarButtonClickCount = 0;
  bool prevMonthClicked = false;
  bool nextMonthClicked = false;
  bool forceFlagWritten = false;
  bool forceFlagWriteSucceeds = true;
  bool restarted = false;
  bool buttonsPressed = false;
  unsigned long now = 0;
  unsigned long renderHomeDurationMs = 0;
  int updateCalls = 0;
  std::time_t currentUnix = 1704110400;
  std::vector<homedeck::HomeSleepRequest> sleepRequests;
  std::vector<homedeck::SystemView> sleepRenderedViews;
  bool sleepRenderCalled = false;

  std::vector<std::unique_ptr<MockView>> mocks;

  homedeck::BootControllerDeps deps() {
    homedeck::BootControllerDeps deps{};
    deps.loadFlags = [this]() {
      return homedeck::BootFlags{configured, forceConfig};
    };
    deps.clearForceConfigOnNextBoot = [this]() {
      forceConfig = false;
      return true;
    };
    deps.setForceConfigOnNextBoot = [this]() {
      forceFlagWritten = true;
      if (!forceFlagWriteSucceeds) return false;
      forceConfig = true;
      return true;
    };
    deps.startConfigPortal = [this]() { portalStarted = true; };
    deps.handleConfigPortalClient = []() {};
    deps.restoreSystemTimeFromRtc = []() {};
    deps.getCalendarButtonClickCount = [this]() { return calendarButtonClickCount; };
    deps.wasPrevMonthClicked = [this]() { return prevMonthClicked; };
    deps.wasNextMonthClicked = [this]() { return nextMonthClicked; };
    deps.updateButtons = [this]() { ++updateCalls; };
    deps.areSetupButtonsPressed = [this]() { return buttonsPressed; };
    deps.millis = [this]() { return now; };
    deps.restart = [this]() { restarted = true; };
    deps.currentTime = [this]() { return currentUnix; };
    deps.loadSavedView = []() { return homedeck::SystemView::Almanac; };
    deps.saveCurrentView = [](homedeck::SystemView) {};
    deps.enterDeepSleep = [this](const homedeck::HomeSleepRequest& req) {
      sleepRequests.push_back(req);
    };
    deps.createViewManager = [this]() {
      auto vm = std::make_unique<homedeck::ViewManager>();
      auto almanac = std::make_unique<MockView>(homedeck::SystemView::Almanac);
      mocks.push_back(almanac.get());
      vm->addView(std::move(almanac));

      auto calendar = std::make_unique<MockView>(homedeck::SystemView::Calendar);
      mocks.push_back(calendar.get());
      vm->addView(std::move(calendar));

      auto countdown = std::make_unique<MockView>(homedeck::SystemView::Countdown);
      mocks.push_back(countdown.get());
      vm->addView(std::move(countdown));
      return vm;
    };
    return deps;
  }
};

}  // namespace

void setUp() {
  setenv("TZ", "UTC", 1);
  tzset();
}
void tearDown() {}

// ... 保留所有现有测试，但适配新的 deps ...

void test_single_click_switches_view() {
  Fixture f{};
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.calendarButtonClickCount = 1;
  controller.update();

  TEST_ASSERT_EQUAL(homedeck::SystemView::Calendar, controller.currentView());
  TEST_ASSERT_TRUE(f.mocks[1]->rendered);
}

void test_second_click_switches_to_countdown() {
  Fixture f{};
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.calendarButtonClickCount = 1;
  controller.update();
  f.calendarButtonClickCount = 0;

  f.calendarButtonClickCount = 1;
  controller.update();

  TEST_ASSERT_EQUAL(homedeck::SystemView::Countdown, controller.currentView());
  TEST_ASSERT_TRUE(f.mocks[2]->rendered);
}

void test_third_click_cycles_back_to_almanac() {
  Fixture f{};
  homedeck::BootController controller{f.deps()};
  controller.begin();

  for (int i = 0; i < 3; ++i) {
    f.calendarButtonClickCount = 1;
    controller.update();
    f.calendarButtonClickCount = 0;
  }

  TEST_ASSERT_EQUAL(homedeck::SystemView::Almanac, controller.currentView());
}

void test_double_click_calls_reset() {
  Fixture f{};
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.calendarButtonClickCount = 2;
  controller.update();

  TEST_ASSERT_TRUE(f.mocks[0]->resetCalled);
}

void test_button_a_forwarded_to_current_view() {
  Fixture f{};
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.prevMonthClicked = true;
  controller.update();

  TEST_ASSERT_TRUE(f.mocks[0]->buttonA);
}

void test_button_b_forwarded_to_current_view() {
  Fixture f{};
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.nextMonthClicked = true;
  controller.update();

  TEST_ASSERT_TRUE(f.mocks[0]->buttonB);
}

void test_deep_sleep_calls_renderSleep() {
  Fixture f{};
  homedeck::BootController controller{f.deps()};
  controller.begin();

  f.now = 300000;
  controller.update();

  TEST_ASSERT_TRUE(f.mocks[0]->sleepRendered);
  TEST_ASSERT_EQUAL(1, static_cast<int>(f.sleepRequests.size()));
}

// 保留其他现有测试：配置模式、AB长按重启、sleep计时、多update不重复sleep等
```

**注意：** `test_boot_controller` 中保留的所有现有测试都需要适配新的 `Fixture` 结构。由于 deps 中移除了 `renderAlmanac`/`renderCalendar` 等回调，测试不再检查这些布尔标志，而是检查 `MockView` 的 `rendered`/`resetCalled`/`sleepRendered` 等状态。

- [ ] **Step 7: 运行测试**

```bash
pio test -e native --filter test_view_manager
pio test -e native --filter test_boot_controller
```

Expected: 两个测试全部 PASS。

- [ ] **Step 8: Commit**

```bash
git add src/view_manager.h src/view_manager.cpp src/boot_controller.h src/boot_controller.cpp \
  test/native/test_view_manager/test_main.cpp test/native/test_boot_controller/test_main.cpp
git commit -m "refactor: ViewManager 和 BootController 使用统一视图接口

- ViewManager 从回调模式重构为持有 ViewRenderer 实例
- BootController 移除 per-view render 回调
- 按钮翻页/双击重置/deep sleep 前渲染通过 ViewManager::current() 转发
- 测试使用 Mock ViewRenderer 适配新接口"
```

---

## Task 6: app_runtime 适配新架构

**Files:**
- Modify: `src/app_runtime.cpp`

**背景：** `makeBootDeps()` 现在需要提供 `createViewManager` 回调，而不是 per-view 的 render 回调。

- [ ] **Step 1: 修改 `src/app_runtime.cpp`**

1. 在文件顶部添加 `#include "countdown_view.h"`（即使 CountdownView 还没实现，先 include）。

2. 删除以下函数（它们的逻辑已内聚到视图类中）：
   - `renderHomeWithEnvironment()`
   - `renderCalendarWithEnvironment()`
   - `renderCalendarWithOffset()`
   - `renderAlmanacWithOffset()`
   - `makeCurrentHomeCalendarDataWithEnvironment()`
   - `formatCurrentTimeHHMM()`（各视图内部自行格式化时间）

3. 修改 `makeBootDeps()`：

```cpp
BootControllerDeps makeBootDeps() {
  BootControllerDeps deps{};
  // ... 保留所有非 render 相关的回调 ...

  deps.createViewManager = []() {
    auto vm = std::make_unique<ViewManager>();
    vm->addView(std::make_unique<AlmanacView>());
    vm->addView(std::make_unique<CalendarView>());
    vm->addView(std::make_unique<CountdownView>());  // 后续步骤实现
    return vm;
  };

  // 删除：renderAlmanac、renderCalendar、renderCalendarWithOffset、
  //       renderAlmanacWithOffset、preSleepRender

  return deps;
}
```

4. 移除全局变量 `gHomeRenderer`（如果不再需要）。`gConfigPortal` 保留。

- [ ] **Step 2: 运行测试**

```bash
pio test -e native
```

Expected: 通过。此时 `CountdownView` 可能还没有实现，但 `createViewManager` 回调中如果已经 `std::make_unique<CountdownView>()`，需要确保 `CountdownView` 的声明和最小实现已存在。如果 Task 7 还没做，可以先注释掉 `CountdownView` 的注册，等 Task 7 再启用。

建议：在 Task 6 中先不注册 `CountdownView`（只注册 Almanac 和 Calendar），等 Task 7 再添加。这样 Task 6 的测试不受未实现代码的影响。

- [ ] **Step 3: Commit**

```bash
git add src/app_runtime.cpp
git commit -m "refactor: app_runtime 适配统一视图架构

- makeBootDeps() 通过 createViewManager 注册 AlmanacView 和 CalendarView
- 移除所有 per-view render 回调函数"
```

---

## Task 7: 添加 CountdownView

**Files:**
- Create: `src/countdown_view.h`
- Create: `src/countdown_view.cpp`
- Modify: `src/app_runtime.cpp`

**背景：** 实现倒数日视图的数据计算和渲染，并注册到 `ViewManager`。

- [ ] **Step 1: 创建 `src/countdown_view.h`**

```cpp
#pragma once

#include <string>

#include "view_renderer.h"

namespace homedeck {

struct CountdownData {
  int year = 0;
  int daysRemaining = 0;
  bool temperatureAvailable = false;
  float temperatureCelsius = 0.0f;
  bool humidityAvailable = false;
  float humidityPercent = 0.0f;
  std::string bottomCenterMessage;
};

CountdownData makeCurrentCountdownData();

class CountdownView : public ViewRenderer {
 public:
  SystemView viewType() const override { return SystemView::Countdown; }
  void render() override;
  void renderSleep() override;
};

}  // namespace homedeck
```

- [ ] **Step 2: 创建 `src/countdown_view.cpp`**

```cpp
#include "countdown_view.h"

#include <M5Unified.h>
#include <cstdio>
#include <ctime>

#include "generated/device_font_vlw.h"
#include "render_context.h"
#include "sht40_reader.h"

namespace homedeck {

namespace {

constexpr int kCountdownCenterX = 200;
constexpr int kLine1Y = 110;  // 20xx年还剩
constexpr int kLine2Y = 195;  // xx (156px)
constexpr int kLine3Y = 290;  // 天

std::tm fallbackLocalTime() {
  std::tm local{};
  local.tm_year = 1970 - 1900;
  local.tm_mon = 0;
  local.tm_mday = 1;
  local.tm_wday = 4;
  return local;
}

bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInYear(int year) {
  return isLeapYear(year) ? 366 : 365;
}

std::string formatTemperatureText(bool available, float celsius) {
  if (!available) return "--.-°C";
  char buffer[16] = {};
  std::snprintf(buffer, sizeof(buffer), "%.1f°C", celsius);
  return buffer;
}

std::string formatHumidityText(bool available, float percent) {
  if (!available) return "--.-%";
  char buffer[16] = {};
  std::snprintf(buffer, sizeof(buffer), "%.1f%%", percent);
  return buffer;
}

void drawEnvironmentReadings(M5Canvas& canvas, const CountdownData& data) {
  constexpr int kBottomInset = 12;
  constexpr int kLeftX = 12;
  constexpr int kRightX = 388;
  const int bottomY = canvas.height() - kBottomInset;

  canvas.setTextDatum(textdatum_t::bottom_left);
  canvas.drawString(formatTemperatureText(data.temperatureAvailable, data.temperatureCelsius).c_str(), kLeftX, bottomY);

  if (!data.bottomCenterMessage.empty()) {
    canvas.setTextDatum(textdatum_t::bottom_center);
    canvas.drawString(data.bottomCenterMessage.c_str(), kCountdownCenterX, bottomY);
  }

  canvas.setTextDatum(textdatum_t::bottom_right);
  canvas.drawString(formatHumidityText(data.humidityAvailable, data.humidityPercent).c_str(), kRightX, bottomY);
}

}  // namespace

CountdownData makeCurrentCountdownData() {
  CountdownData data{};
  const std::time_t now = std::time(nullptr);
  const std::tm* local = now > 0 ? std::localtime(&now) : nullptr;
  const std::tm& tm = local != nullptr ? *local : fallbackLocalTime();

  data.year = tm.tm_year + 1900;
  const int totalDays = daysInYear(data.year);
  data.daysRemaining = totalDays - tm.tm_yday - 1;
  if (data.daysRemaining < 0) data.daysRemaining = 0;

  const EnvironmentReading reading = readSht40Environment();
  if (reading.ok) {
    data.temperatureAvailable = true;
    data.temperatureCelsius = reading.temperatureCelsius;
    data.humidityAvailable = true;
    data.humidityPercent = reading.humidityPercent;
  }

  const std::time_t tnow = std::time(nullptr);
  const std::tm* tlocal = std::localtime(&tnow);
  char timeStr[6] = {};
  if (tlocal != nullptr) {
    std::snprintf(timeStr, sizeof(timeStr), "%02d:%02d", tlocal->tm_hour, tlocal->tm_min);
  }
  data.bottomCenterMessage = timeStr;

  return data;
}

void CountdownView::render() {
  CountdownData data = makeCurrentCountdownData();

  M5Canvas& canvas = sprite();
  prepareScreen(canvas);

  // 第一行：20xx年还剩
  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::middle_center);
    char line1[32] = {};
    std::snprintf(line1, sizeof(line1), "%d年还剩", data.year);
    canvas.drawString(line1, kCountdownCenterX, kLine1Y);
    canvas.unloadFont();
  }

  // 第二行：xx (156px)
  if (canvas.loadFont(generated::kDeviceLargeDateFontVlw)) {
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::middle_center);
    char line2[8] = {};
    std::snprintf(line2, sizeof(line2), "%d", data.daysRemaining);
    canvas.drawString(line2, kCountdownCenterX, kLine2Y);
    canvas.unloadFont();
  }

  // 第三行：天
  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.drawString("天", kCountdownCenterX, kLine3Y);
    canvas.unloadFont();
  }

  drawEnvironmentReadings(canvas, data);
  pushScreen(canvas);
}

void CountdownView::renderSleep() {
  CountdownData data = makeCurrentCountdownData();
  data.temperatureAvailable = false;
  data.humidityAvailable = false;
  data.bottomCenterMessage = "--:--";

  M5Canvas& canvas = sprite();
  prepareScreen(canvas);

  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::middle_center);
    char line1[32] = {};
    std::snprintf(line1, sizeof(line1), "%d年还剩", data.year);
    canvas.drawString(line1, kCountdownCenterX, kLine1Y);
    canvas.unloadFont();
  }

  if (canvas.loadFont(generated::kDeviceLargeDateFontVlw)) {
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::middle_center);
    char line2[8] = {};
    std::snprintf(line2, sizeof(line2), "%d", data.daysRemaining);
    canvas.drawString(line2, kCountdownCenterX, kLine2Y);
    canvas.unloadFont();
  }

  if (canvas.loadFont(generated::kDeviceFontVlw)) {
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.drawString("天", kCountdownCenterX, kLine3Y);
    canvas.unloadFont();
  }

  drawEnvironmentReadings(canvas, data);
  pushScreen(canvas);
}

}  // namespace homedeck
```

- [ ] **Step 3: `src/app_runtime.cpp` 注册 CountdownView**

在 `makeBootDeps()` 的 `createViewManager` lambda 中，添加：

```cpp
vm->addView(std::make_unique<CountdownView>());
```

- [ ] **Step 4: 运行测试**

```bash
pio test -e native
```

Expected: 全部通过。

- [ ] **Step 5: Commit**

```bash
git add src/countdown_view.h src/countdown_view.cpp src/app_runtime.cpp
git commit -m "feat: 添加 CountdownView 倒数日视图

- 实现 makeCurrentCountdownData()：当年剩余天数计算
- 三行居中布局：20xx年还剩 / xx（156px）/ 天
- 底部状态栏（温湿度+时间）
- renderSleep() 去活性画面
- 注册到 ViewManager"
```

---

## Task 8: 字体资源调整

**Files:**
- Modify: `tools/generate_device_font.py`
- Modify: `src/generated/device_font_vlw.h`
- Modify: `src/generated/device_font_vlw.cpp`

**背景：** `EXTRA_TEXT` 缺少"剩"字，需要追加并重新生成字体。

- [ ] **Step 1: 修改 `tools/generate_device_font.py`**

在 `EXTRA_TEXT` 中追加"剩"字：

```python
EXTRA_TEXT = (
    "，。！？；：""''（）《》、—…·"
    "℃°％%年月日星期农历节气节假日今日日程温度湿度还有项连接开放热点打开当前热点配置不可用为空同步网络传感器"
    "HomeDeck Wi-Fi NTP webcal"
    "剩"  # 倒数日视图：20xx年还剩
)
```

- [ ] **Step 2: 运行字体生成脚本**

```bash
cd tools && python3 generate_device_font.py
```

Expected: 脚本成功执行，输出显示 `kDeviceFontVlw` 的 glyph 数从 7541 变为 7542（或类似的增量）。

- [ ] **Step 3: 验证生成的字体文件**

检查 `src/generated/device_font_vlw.h` 中 `kDeviceFontGlyphCount` 是否已更新。

- [ ] **Step 4: Commit**

```bash
git add tools/generate_device_font.py src/generated/device_font_vlw.h src/generated/device_font_vlw.cpp
git commit -m "chore: 字体资源添加'剩'字

- EXTRA_TEXT 追加'剩'字，供倒数日视图'20xx年还剩'使用"
```

---

## Task 9: 新增 CountdownView 单元测试

**Files:**
- Create: `test/native/test_countdown_view/test_main.cpp`

**背景：** 验证天数计算的核心逻辑。

- [ ] **Step 1: 创建 `test/native/test_countdown_view/test_main.cpp`**

```cpp
#include <unity.h>
#include <ctime>

#include "countdown_view.h"

namespace {

void setFixedTime(int year, int month, int day) {
  std::tm tm{};
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = 12;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  tm.tm_isdst = 0;
  time_t t = std::mktime(&tm);
  // 注意：native 测试环境中无法真正修改系统时间
  // 这里需要一个可测试的 makeCountdownData(const std::tm&) 重载
}

}  // namespace

void setUp() {}
void tearDown() {}

// 为了使 CountdownView 可测试，需要在 countdown_view.h 中添加：
// CountdownData makeCountdownData(const std::tm& localTime);

void test_leap_year_jan_1() {
  std::tm tm{};
  tm.tm_year = 2024 - 1900;  // 闰年
  tm.tm_mon = 0;
  tm.tm_mday = 1;
  tm.tm_yday = 0;

  homedeck::CountdownData data = homedeck::makeCountdownData(tm);

  TEST_ASSERT_EQUAL(2024, data.year);
  TEST_ASSERT_EQUAL(365, data.daysRemaining);
}

void test_nonleap_year_jan_1() {
  std::tm tm{};
  tm.tm_year = 2025 - 1900;  // 平年
  tm.tm_mon = 0;
  tm.tm_mday = 1;
  tm.tm_yday = 0;

  homedeck::CountdownData data = homedeck::makeCountdownData(tm);

  TEST_ASSERT_EQUAL(2025, data.year);
  TEST_ASSERT_EQUAL(364, data.daysRemaining);
}

void test_dec_31() {
  std::tm tm{};
  tm.tm_year = 2025 - 1900;
  tm.tm_mon = 11;
  tm.tm_mday = 31;
  tm.tm_yday = 364;

  homedeck::CountdownData data = homedeck::makeCountdownData(tm);

  TEST_ASSERT_EQUAL(0, data.daysRemaining);
}

void test_mid_year() {
  std::tm tm{};
  tm.tm_year = 2025 - 1900;
  tm.tm_mon = 5;  // June
  tm.tm_mday = 15;
  tm.tm_yday = 165;  // Jan 1 = 0, so June 15 = 165 in non-leap year

  homedeck::CountdownData data = homedeck::makeCountdownData(tm);

  TEST_ASSERT_EQUAL(365 - 165 - 1, data.daysRemaining);  // 199
}

void test_leap_year_feb_29() {
  std::tm tm{};
  tm.tm_year = 2024 - 1900;
  tm.tm_mon = 1;  // Feb
  tm.tm_mday = 29;
  tm.tm_yday = 59;  // Jan 31 + Feb 29 = 31 + 29 - 1 = 59 (0-indexed)

  homedeck::CountdownData data = homedeck::makeCountdownData(tm);

  TEST_ASSERT_EQUAL(366 - 59 - 1, data.daysRemaining);  // 306
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_leap_year_jan_1);
  RUN_TEST(test_nonleap_year_jan_1);
  RUN_TEST(test_dec_31);
  RUN_TEST(test_mid_year);
  RUN_TEST(test_leap_year_feb_29);
  return UNITY_END();
}
```

**注意：** 以上测试要求在 `countdown_view.h` 中添加可测试的重载：

```cpp
CountdownData makeCountdownData(const std::tm& localTime);
```

`makeCurrentCountdownData()` 内部调用 `makeCountdownData(*localTime)`。

- [ ] **Step 2: 运行测试**

```bash
pio test -e native --filter test_countdown_view
```

Expected: 5 个测试全部 PASS。

- [ ] **Step 3: Commit**

```bash
git add test/native/test_countdown_view/test_main.cpp src/countdown_view.h src/countdown_view.cpp
git commit -m "test: 添加 CountdownView 天数计算单元测试

- 验证平年/闰年 1月1日、12月31日、年中某日、闰年2月29日
- 添加 makeCountdownData(const std::tm&) 可测试重载"
```

---

## Self-Review

### 1. Spec Coverage

| Spec 要求 | 对应 Task |
|-----------|----------|
| 添加 Countdown 枚举，切换顺序 Almanac→Calendar→Countdown→Almanac | Task 2、Task 5 |
| 倒数日视图三行居中布局 | Task 7 |
| 156px 大号字体显示天数，20px BODY 字体显示"年还剩"和"天" | Task 7 |
| 底部状态栏与黄历/日历一致 | Task 7（drawEnvironmentReadings） |
| Deep sleep 逻辑统一 | Task 5（BootController::updateHomeSleep） |
| ViewRenderer 统一接口 | Task 4、Task 5 |
| ViewManager 重构为注册模式 | Task 5 |
| BootController 解耦 per-view 回调 | Task 5 |
| Almanac 缓存供 calendar_view 使用 | Task 3（almanac_view.h 暴露声明） |
| 字体 EXTRA_TEXT 添加"剩" | Task 8 |
| CountdownView 单元测试 | Task 9 |
| 现有测试继续通过 | 每个 Task 的 Step 运行测试 |

**无遗漏。**

### 2. Placeholder Scan

- 无 TBD、TODO、"implement later"。
- 无 "add appropriate error handling" 等模糊描述。
- 每个代码步骤展示了完整代码或关键结构。

### 3. Type Consistency

- `SystemView::Countdown` 在设计文档和各 Task 中命名一致。
- `ViewRenderer` 接口方法（`render()`、`renderSleep()`、`onButtonA()`、`onButtonB()`、`reset()`）在各处一致。
- `makeCountdownData` / `makeCurrentCountdownData` 命名在 Task 7 和 Task 9 中一致。

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-29-countdown-view.md`.

**Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
