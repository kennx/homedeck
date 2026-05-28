# 黄历按天翻页功能设计文档

## 背景

日历视图已实现翻页（BtnB 上一月 / BtnA 下一月 / BtnC 双击回本月）。用户要求黄历（Almanac）视图实现对称的翻页能力，但黄历是单日视图，因此按**天**翻页。

## 目标

- 黄历视图下支持前后翻页浏览任意日期的黄历数据
- 操作方式与日历翻页保持一致：BtnB 上一天、BtnA 下一天、BtnC 双击回今天
- 深睡眠唤醒后自动回到今天
- 最小化改动，复用现有架构和模式

## 架构设计

采用**独立偏移状态**方案，与日历翻页的 `calendarMonthOffset_` 完全对称。

### 状态管理

| 状态 | 类型 | 初始值 | 范围 | 说明 |
|---|---|---|---|---|
| `calendarMonthOffset_` | `int` | `0` | `[-120, 120]` | 已有：日历月偏移 |
| `almanacDayOffset_` | `int` | `0` | `[-3650, 3650]` | **新增**：黄历天偏移 |

**生命周期：**
- `enterSystemMode()` 时重置为 `0`（深睡眠唤醒后回到今天）
- `update()` 中 Almanac 视图下响应按钮事件
- 翻页后重置 `lastActivityMs_`（保持现有睡眠计时逻辑）

### 数据流

```
BtnA/B clicked
  → BootController::update()
  → almanacDayOffset_ += ±1（边界内）
  → deps.renderAlmanacWithOffset(offset)
  → app_runtime::renderAlmanacWithOffset
     → time(nullptr) + offset × 86400 → localtime
     → makeHomeCalendarData(tm)   // 已有函数
     → 附加实时温湿度
     → gHomeRenderer.render(data)
```

温湿度保持实时读取（和日历翻页时一样），因为环境数据不是历史数据。

## 组件改动

### `boot_controller.h`

- 新增依赖：`deps.renderAlmanacWithOffset`（`std::function<void(int)>`）
- 新增成员：`int almanacDayOffset_ = 0;`
- 新增常量：`kAlmanacDayOffsetMin = -3650`，`kAlmanacDayOffsetMax = 3650`

### `boot_controller.cpp`

- `enterSystemMode()`：新增 `almanacDayOffset_ = 0;`
- `update()`：
  - **BtnC 双击**：当前只在 `Calendar` 视图回本月，扩展为 `Almanac` 视图也回今天
  - **BtnA/B 翻页**：当前只在 `Calendar` 视图检测，新增 `Almanac` 视图分支，按天增减 `almanacDayOffset_`，边界外忽略

### `app_runtime.cpp`

- 新增函数 `renderAlmanacWithOffset(int dayOffset)`：
  - 当前时间 + `dayOffset × 86400` → `localtime`
  - 调用 `makeHomeCalendarData(tm)` 生成黄历数据
  - 附加 SHT40 温湿度
  - 调用 `gHomeRenderer.render(data)`
- `makeBootDeps()`：绑定 `deps.renderAlmanacWithOffset = renderAlmanacWithOffset;`

### 无改动文件

- `home_renderer.h / .cpp`：`render(const HomeCalendarData&)` 已支持任意日期
- `view_manager.h / .cpp`：视图切换逻辑无需变更

## 交互行为

| 视图 | BtnA | BtnB | BtnC 单击 | BtnC 双击 |
|---|---|---|---|---|
| Almanac | 下一天 (+1) | 上一天 (-1) | 切换到 Calendar | **回今天**（重置偏移） |
| Calendar | 下一月 (+1) | 上一月 (-1) | 切换到 Almanac | 回本月（重置偏移） |

- 两个视图的偏移相互独立。黄历翻 3 天后切到日历，日历仍显示本月。
- 超出 `±3650` 天边界后，继续按翻页按钮无响应（和日历一致）。

## 错误处理

| 场景 | 处理方式 |
|---|---|
| `localtime` 返回 `nullptr` | 回退到今天的数据，避免崩溃 |
| 超出黄历数据文件范围 | `AlmanacProvider::lookup` 失败，显示"数据缺失"（已有行为） |
| 超出偏移边界 `±3650` | 忽略按钮事件，不更新偏移 |

## 测试策略

### `test/native/test_boot_controller/test_main.cpp`

| 测试名 | 验证内容 |
|---|---|
| `test_prev_day_click_in_almanac` | Almanac 视图下 BtnB → `renderAlmanacWithOffset(-1)` |
| `test_next_day_click_in_almanac` | Almanac 视图下 BtnA → `renderAlmanacWithOffset(1)` |
| `test_day_click_ignored_in_calendar` | Calendar 视图下 BtnA/B 不触发黄历翻页 |
| `test_double_click_resets_to_today_in_almanac` | Almanac 视图下 BtnC 双击 → 偏移重置为 0 |
| `test_continuous_prev_day_clicks` | 连续按 BtnB，偏移累计到 -3 |
| `test_day_click_resets_sleep_timer` | 翻页后 300s 内不进入深睡眠 |
| `test_enter_system_mode_resets_day_offset` | 重新 `begin()` 后偏移归零 |
| `test_almanac_day_bounds` | 达到 `±3650` 边界后继续按按钮，偏移不再变化 |

### `test/native/test_home_renderer/test_main.cpp`

无需新增测试。`render(const HomeCalendarData&)` 已有完整覆盖，翻页只是传入不同的日期数据。

## 成功标准

1. Almanac 视图下 BtnA/B 可按天翻页，渲染对应日期的黄历数据
2. BtnC 双击在黄历视图下回到今天
3. 深睡眠唤醒后自动回到今天
4. 所有 native 单元测试通过
5. ESP32-S3 真机编译成功
