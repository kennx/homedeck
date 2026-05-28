# Deep Sleep 显示优化设计

## 背景

当前设备进入 deep sleep 前，统一调用 `renderHomeWithDeepSleepMessage()` 在黄历底部显示 `"DEEP SLEEP"` 文字，然后屏幕休眠。用户反馈希望优化以下三点：

1. 去掉 `"DEEP SLEEP"` 文字
2. 底部中间改为显示当前时间 `HH:MM`（24小时制），只在唤醒和刷新屏幕时刷新
3. 日历视图下进入 deep sleep 时显示日历（而非黄历），且重置回本月

## 设计目标

- 深睡眠前最后一帧画面更实用：显示当前时间 + 保持当前视图
- 日历模式下睡眠自动回到本月，唤醒后继续显示本月
- 保持现有架构的解耦，不引入不必要的耦合

## 架构设计

采用 **方案 B：注入 `preSleepRender` 回调**。

`BootController` 负责"何时进入睡眠"和"重置状态"，但不负责"睡眠前画什么"。渲染逻辑集中在 `app_runtime` 集成层。

```
┌─────────────────┐     preSleepRender(SystemView)      ┌─────────────────┐
│  BootController │ ───────────────────────────────────> │   app_runtime   │
│                 │                                     │ (preSleepRender │
│  1. 重置 offset │                                     │   lambda 实现)  │
│  2. 调用回调     │                                     │                 │
│  3. 进入 sleep  │ ──enterDeepSleep(HomeSleepRequest)──> │ enterHomeDeepSleep
└─────────────────┘                                     └─────────────────┘
```

## 详细设计

### 1. BootController 改动

#### `BootControllerDeps` 新增回调

```cpp
struct BootControllerDeps {
  // ... 现有依赖 ...
  std::function<void(SystemView)> preSleepRender;
};
```

#### `BootController::updateHomeSleep()` 流程

超时触发 deep sleep 时的执行顺序：

1. `calendarMonthOffset_ = 0`
2. `almanacDayOffset_ = 0`
3. `deps_.preSleepRender(currentView())`
4. `deps_.enterDeepSleep(makeHomeSleepRequest())`

### 2. 数据模型改动

#### `CalendarData` 新增字段

```cpp
struct CalendarData {
  // ... 现有字段 ...
  std::string bottomCenterMessage;
};
```

与 `HomeCalendarData` 对齐，使日历视图也支持底部中间自定义文字。

#### `drawEnvironmentReadings()` 通用化

从硬判断 `"DEEP SLEEP"` 改为判断非空字符串：

```cpp
// 修改前
if (data.bottomCenterMessage == "DEEP SLEEP") { ... }

// 修改后
if (!data.bottomCenterMessage.empty()) { ... }
```

### 3. HomeRenderer 改动

#### `renderCalendar()` 传递 `bottomCenterMessage`

构造传给 `drawEnvironmentReadings` 的临时 `HomeCalendarData` 时，把 `CalendarData.bottomCenterMessage` 一并带过去：

```cpp
HomeCalendarData envData{};
envData.temperatureAvailable = data.temperatureAvailable;
// ... 其他温湿度字段 ...
envData.bottomCenterMessage = data.bottomCenterMessage;  // 新增
drawEnvironmentReadings(canvas, envData);
```

### 4. app_runtime 正常渲染路径改动

#### 新增 `formatCurrentTimeHHMM()` 辅助函数

```cpp
std::string formatCurrentTimeHHMM() {
  time_t now = time(nullptr);
  tm* local = localtime(&now);
  char timeStr[6] = {};
  if (local != nullptr) {
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", local->tm_hour, local->tm_min);
  }
  return timeStr;
}
```

#### 正常渲染函数统一设置 `bottomCenterMessage`

所有活跃状态下的渲染函数（初始渲染、翻页、视图切换）都在底部中间显示当前 `HH:MM`：

- `renderHomeWithEnvironment()` — 传入 `formatCurrentTimeHHMM()`
- `renderCalendarWithEnvironment()` — 设置 `data.bottomCenterMessage = formatCurrentTimeHHMM()`
- `renderCalendarWithOffset()` — 设置 `data.bottomCenterMessage = formatCurrentTimeHHMM()`
- `renderAlmanacWithOffset()` — 设置 `data.bottomCenterMessage = formatCurrentTimeHHMM()`

### 5. app_runtime preSleepRender 实现

在 `makeBootDeps()` 中绑定 `preSleepRender` lambda：

```cpp
deps.preSleepRender = [](SystemView view) {
  if (view == SystemView::Almanac) {
    HomeCalendarData data = makeCurrentHomeCalendarDataWithEnvironment("--:--");
    data.temperatureAvailable = false;
    data.humidityAvailable = false;
    gHomeRenderer.render(data);
  } else {
    CalendarData data = makeCurrentCalendarData();
    data.bottomCenterMessage = "--:--";
    data.temperatureAvailable = false;
    data.humidityAvailable = false;
    gHomeRenderer.renderCalendar(data);
  }
};
```

### 6. enterHomeDeepSleep 简化

移除 `renderHomeWithDeepSleepMessage()` 调用，不再负责渲染：

```cpp
void enterHomeDeepSleep(const HomeSleepRequest& request) {
  // GPIO/RTC 唤醒配置（不变）
  // ...
  // renderHomeWithDeepSleepMessage();  // 删除此行
  M5.Display.sleep();
  M5.Display.waitDisplay();
  M5.Power.deepSleep(request.timerWakeupUs, false);
}
```

`renderHomeWithDeepSleepMessage()` 函数一并删除。

## 交互行为

| 当前视图 | 进入 deep sleep 前显示 | 底部（左/中/右） | 唤醒后状态 |
|---------|----------------------|----------------|----------|
| 黄历（本月） | 今天黄历 | `--.-°C` / `--:--` / `--.-%` | 黄历（本月），显示实际温湿度和 `HH:MM` |
| 黄历（翻页后） | 今天黄历 | `--.-°C` / `--:--` / `--.-%` | 黄历（本月，offset 已重置），显示实际温湿度和 `HH:MM` |
| 日历（本月） | 本月日历 | `--.-°C` / `--:--` / `--.-%` | 日历（本月），显示实际温湿度和 `HH:MM` |
| 日历（翻页后） | 本月日历（offset 重置） | `--.-°C` / `--:--` / `--.-%` | 日历（本月，offset 已重置），显示实际温湿度和 `HH:MM` |

## 测试策略

### BootController 测试

1. **`test_preSleepRender_called_before_deep_sleep`**
   - 模拟超时条件
   - 验证 `preSleepRender` 被调用，且传入的 `SystemView` 与当前视图一致
   - 验证 `enterDeepSleep` 在 `preSleepRender` 之后被调用

2. **`test_offsets_reset_before_deep_sleep`**
   - 设置 `calendarMonthOffset_ = 5`，`almanacDayOffset_ = 10`
   - 模拟超时进入 deep sleep
   - 验证两个 offset 在 `preSleepRender` 之前已被重置为 0

3. **`test_preSleepRender_almanac_view`**
   - 当前视图为 Almanac
   - 验证 `preSleepRender` 收到 `SystemView::Almanac`

4. **`test_preSleepRender_calendar_view`**
   - 当前视图为 Calendar
   - 验证 `preSleepRender` 收到 `SystemView::Calendar`

### HomeRenderer 测试

- `drawEnvironmentReadings` 的通用化改动通过现有 `test_render_displays_bottom_center_message` 覆盖（把测试数据从 `"DEEP SLEEP"` 改为 `"14:30"`）
- 新增 `test_renderCalendar_displays_bottom_center_message` 验证日历视图下底部中间文字也能正确显示

## 变更文件清单

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| `src/boot_controller.h` | 修改 | `BootControllerDeps` 新增 `preSleepRender` |
| `src/boot_controller.cpp` | 修改 | `updateHomeSleep()` 重置 offset 并调用 `preSleepRender` |
| `src/home_renderer.h` | 修改 | `CalendarData` 新增 `bottomCenterMessage` |
| `src/home_renderer.cpp` | 修改 | `drawEnvironmentReadings()` 通用化 + `renderCalendar()` 传递字段 |
| `src/app_runtime.cpp` | 修改 | 绑定 `preSleepRender` lambda + 删除 `renderHomeWithDeepSleepMessage()` + 简化 `enterHomeDeepSleep()` |
| `test/native/test_boot_controller/test_main.cpp` | 修改 | 新增 4 个测试 + Fixture 新增 mock 桩 |
| `test/native/test_home_renderer/test_main.cpp` | 修改 | 更新现有测试数据 + 新增日历底部文字测试 |
| `test/native/test_app_runtime/test_main.cpp` | 修改 | 更新 deep sleep 测试（移除渲染断言，调整 waitDisplayCount） |
