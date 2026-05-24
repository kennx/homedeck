# HomeDeck 配置模式设计

日期：2026-05-24
状态：已确认，待实现计划

## 背景

当前 `dev` 分支已经回到基础 PlatformIO 骨架，`src/main.cpp` 只负责 `M5.begin()` 和 `M5.update()`。这次设计只实现首次配置、启动后按键进入配置、RTC 校准和临时首页显示，不恢复旧 `main` 分支里的日历、传感器、完整首页等能力。

设备是 M5Stack PaperColor，使用 Arduino + M5Unified + M5GFX + M5PM1。PaperColor 文档记录 `USER_KEY3` 是 Button A，`USER_KEY2` 是 Button B，屏幕为 400x600，目标首页暂时只有 portrait 模式。

## 目标

1. 首次启动或被一次性配置标记触发时进入配置模式。
2. 设备启动开放 AP，SSID 为 `HomeDeck-XXXX`，用户通过 `192.168.4.1` 打开配置页。
3. 配置页支持 Wi-Fi 扫描前 5 条、Wi-Fi SSID/密码、时区、自动纠正 RTC、NTP 服务器、手动日期时间。
4. Wi-Fi SSID 和密码都允许为空；无 Wi-Fi 时必须手动设置时间。
5. 保存成功后 1 秒自动重启，然后进入系统。
6. 已配置设备进入系统后显示 `HomeDeck`，portrait 模式下水平和垂直居中。
7. 系统界面出现后 5 秒内监听 Button A + Button B，同时按住满 3 秒后先重启，再进入配置模式。

## 非目标

- 不实现日历、温湿度、新闻、图片或语音播报。
- 不实现运行中长期热键监听；A+B 只在系统界面出现后的 5 秒窗口内有效。
- 不实现周期性 NTP 校准；这次只在配置保存时校准 RTC。
- 不要求配置 AP 密码。
- 不引入 captive portal 或 DNS 劫持；用户直接访问 `192.168.4.1`。

## 方案选择

采用轻量状态机 + 小模块拆分。这个方案比单文件实现更可测试，也比恢复旧 `main` 架构更贴合当前 `dev` 的简化状态。

模块边界：

- `BootController`：启动模式判定、一次性配置标记消费、系统渲染、A+B 监听窗口、重启进入配置。
- `ConfigStore`：使用 `Preferences` 保存配置、已配置标记、一次性配置模式标记。
- `ConfigPortal`：开放 AP、Wi-Fi 扫描、配置页面、提交校验、保存成功页面、延迟重启。
- `TimeService`：配置保存时用 NTP 或手动时间校准 RTC；系统启动时从 RTC 恢复系统时间。
- `HomeRenderer`：portrait 模式下绘制居中的 `HomeDeck`。

## 启动与模式切换

启动流程：

1. `M5.begin()` 完成后创建 `BootController`。
2. `BootController` 读取 `ConfigStore`。
3. 如果未配置，进入配置模式。
4. 如果存在一次性配置模式标记，清除标记并进入配置模式。
5. 否则进入系统模式，渲染 `HomeDeck`。

系统模式渲染完成后启动 5 秒监听窗口。窗口内持续调用 `M5.update()` 并检查 Button A 和 Button B。两键同时按下时开始累计；松开任一键则重新计时。累计满 3 秒后写入一次性配置模式标记并调用 `ESP.restart()`。下次启动消费该标记，进入配置模式。

这样用户不需要在上电前预先按住按键，也不会在日常使用中误触配置入口。

## 配置模式 AP

配置模式启动开放 AP：

- SSID：`HomeDeck-XXXX`。
- `XXXX` 使用稳定设备后缀，例如 MAC 地址后 4 位。
- 不设置 AP 密码。
- 访问地址：默认 `192.168.4.1`。

实现阶段需用文档确认 Arduino-ESP32 的 `WiFi.softAP(...)`、`WiFi.softAPIP()`、`WiFi.scanNetworks()`、`WiFi.SSID(i)`、`WiFi.RSSI(i)`、`WiFi.scanDelete()` 等 API。根据 Context7 查询，Arduino-ESP32 支持 `WiFi.softAP(ssid, password)`、`softAPIP()` 和 Wi-Fi 扫描结果读取；扫描数据处理后应释放。

## 配置页面

根路径 `/` 显示 HTML 表单。页面包含：

- Wi-Fi 列表：配置模式启动后扫描附近网络，按 RSSI 从强到弱取前 5 条；点击后填入 SSID。
- Wi-Fi SSID：可为空。
- Wi-Fi 密码：可为空。
- 时区：默认 `Asia/Shanghai`，用于 NTP 后的本地显示和手动时间转换。
- 自动纠正 RTC：checkbox 或 switch；只有 SSID 非空时可用，SSID 为空时禁用。
- NTP 服务器：默认 `pool.ntp.org`；只有自动纠正开启时参与校准。
- 手动日期时间：用于离线配置或 NTP 失败兜底。

页面 JavaScript 只做表单体验增强：SSID 为空时禁用自动纠正开关；禁用时提醒用户填写手动日期时间。最终规则仍由固件端校验。

## 保存与校验

提交路径为 `/save`。保存逻辑：

1. 读取表单字段。
2. 执行配置校验。
3. 按规则校准 RTC。
4. 写入配置和已配置标记。
5. 显示保存成功页面。
6. 1 秒后 `ESP.restart()`。

校验规则：

- Wi-Fi SSID 为空：自动纠正不可用，手动日期时间必填。
- Wi-Fi SSID 非空且自动纠正开启：NTP 服务器必填；手动日期时间可选，作为 NTP 失败兜底。
- Wi-Fi SSID 非空但自动纠正关闭：手动日期时间必填。
- Wi-Fi 密码永远允许为空。
- 时区必填；初版至少支持 `Asia/Shanghai`，实现可复用小型 IANA 到 POSIX 映射。
- 手动日期时间必须能解析成有效日期时间。

错误处理：

- NVS/Preferences 保存失败：停留在配置页并显示错误。
- 自动纠正开启但 Wi-Fi 连接失败：有手动时间就兜底写 RTC；没有手动时间就显示错误。
- NTP 失败：有手动时间就兜底写 RTC；没有手动时间就显示错误。
- RTC 不可用或写入失败：显示错误，不写入已配置标记。

## 时间与 RTC

`TimeService` 负责两类行为：

配置保存时：

- 如果启用自动纠正，先连接 Wi-Fi，再调用 Arduino-ESP32 NTP API 同步系统时间。
- NTP 成功后，将 UTC 时间写入 RTC。
- 如果自动纠正未启用，或自动纠正失败且有手动时间，则按配置时区解释手动时间，转换为 UTC 后写入 RTC。
- 自动纠正失败且没有手动时间时，返回错误。

系统启动时：

- 如果 RTC 可用且未处于低电量无效状态，从 RTC 恢复系统时间。
- 不做后台 NTP 校准。

实现阶段需再次确认 M5Unified 的 RTC API。Context7 当前查询结果显示可使用 `M5.Rtc.isEnabled()`、`M5.Rtc.getVoltLow()`、`M5.Rtc.setSystemTimeFromRtc()`、`M5.Rtc.setDateTime(...)`，并且 NTP 成功后可用 `gmtime(&t)` 生成 UTC 时间写入 RTC。

## 首页显示

`HomeRenderer` 只做临时首页：

- 强制 portrait 模式。
- 清屏。
- 绘制文本 `HomeDeck`。
- 水平和垂直居中。

实现阶段需确认 M5GFX 的居中 API。Context7 当前查询结果显示 `setRotation()` 可设置方向，`setTextDatum(textdatum_t::middle_center)` 配合 `drawString(text, width()/2, height()/2)` 可做居中绘制。

## 数据模型

保存的配置字段：

- `wifiSsid`
- `wifiPassword`
- `timezoneIana`
- `autoRtcCorrection`
- `ntpServer`
- `configured`
- `forceConfigOnNextBoot`

手动日期时间只用于本次保存时写 RTC，不持久化为配置项。

## 测试计划

Native 单元测试覆盖可测逻辑：

- 未配置时进入配置模式。
- 已配置且无一次性标记时进入系统模式。
- 一次性配置模式标记只消费一次。
- 系统渲染后 5 秒窗口内 A+B 同时按住满 3 秒会写标记并请求重启。
- A+B 未满 3 秒或超过窗口不会触发。
- SSID 为空时手动时间必填。
- SSID 为空时自动纠正被视为不可用。
- 密码为空可通过校验。
- 自动纠正开启时 NTP 成功写 RTC。
- 自动纠正失败且有手动时间时写手动时间。
- 自动纠正失败且无手动时间时返回错误。
- 首页使用 portrait 模式并绘制居中的 `HomeDeck`。

构建验证：

- `pio test -e native`
- `pio run -e m5stack-papercolor`

硬件相关 API 用 thin wrapper 或 dependency injection 隔离，native fake 只模拟业务需要的行为。

## 实现注意事项

- 修改保持小步、可测，不恢复旧架构中的非目标能力。
- 所有不确定的 M5Unified、M5GFX、Arduino-ESP32 API 必须在实现前用 Context7、GitHub 源码或搜索工具确认。
- AP 配置页的 HTML 保持简单，避免引入前端构建链。
- 密码字段保存到 NVS；当前设计不加密，后续如有安全需求再扩展。
- 配置保存成功后通过重启进入系统，避免 AP、STA、WebServer 和 RTC 状态残留。
