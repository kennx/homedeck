# HomeDeck 设备默认字体设计

## 背景

HomeDeck 当前设备端渲染器按文本类型手动切换字体：中文文案使用 `fonts::efontCN_14`，ASCII、数字和 IP 等内容使用 M5GFX 的内置默认字体。这个策略导致设备界面字体风格不统一，也让后续新闻、日程标题、农历和状态文本更容易遇到缺字或宽度估算差异。

用户希望以 `fonts/NotoSansSC-Regular.ttf` 作为设备端全局默认字体，不包含网页配置页，并尽可能覆盖常用简体中文。已确认采用 GB2312/常用简体中文覆盖方向。

## 目标

- 使用 `fonts/NotoSansSC-Regular.ttf` 作为设备端字体源。
- 生成 GB2312 子集的 Noto Sans SC 设备字体资源，覆盖常用简体中文、ASCII、数字、常用中英文标点、全角标点和设备界面所需符号。
- 首页和配网页设备屏幕默认使用同一套 Noto Sans SC 字体。
- 不修改网页配置页 HTML/CSS 字体。
- 不直接把 10MB 原始 TTF 放进固件或当前 SPIFFS 分区。
- 保持构建和刷机流程简单，优先随固件内嵌字体资源。

## 非目标

- 不引入多字体 fallback 链。
- 不改日历、传感器、网络、配网表单等非字体逻辑。
- 不追求 Noto Sans SC 全量 Unicode 覆盖。
- 不要求运行时从网络下载字体。
- 不要求本次重新设计页面布局。

## 约束与事实

- 项目使用 PlatformIO、Arduino、M5Unified、M5GFX。
- 当前 M5GFX 版本为 1.2.21，对应 PlatformIO 库版本 `M5GFX @ 0.2.21`。
- M5GFX 的 `setFont(nullptr)` 会回到内置 `fonts::Font0`，不能表达 HomeDeck 设备默认字体。
- M5GFX 支持运行时加载 VLW 字体，`M5Canvas` 需要在自己的绘图上下文中加载或设置字体。
- 当前 `default_16MB.csv` 单个 OTA app 分区约 6.25MB，SPIFFS 约 3.375MB。
- 当前固件约 1.72MB，原始 `NotoSansSC-Regular.ttf` 约 10MB。

## 方案

### 字体资源

新增一个字体生成脚本，从 `fonts/NotoSansSC-Regular.ttf` 生成 M5GFX 可加载的 GB2312 子集 VLW 字体，再把 VLW 转成 C/C++ 字节数组供固件链接。

字表由以下来源合并：

- GB2312 可解码字符。
- ASCII `0x20` 到 `0x7E`。
- 常用中文标点、全角标点、数字相关符号和温度符号。
- 设备端源码和 native 测试中的中文文案。

字体基础字号先采用 14px 级别，尽量贴近当前 `efontCN_14` 的布局尺度。渲染器可以继续通过 `setTextSize()` 控制不同文本层级，但需要重新校准时间和指标数字的字号，避免用 Noto 14px 字体时沿用旧 `Font0` 缩放导致文字过大。

生成产物放在 `src/generated/`。源字体保留在 `fonts/NotoSansSC-Regular.ttf`。

### 设备字体 API

新增一个很薄的设备字体模块，负责表达 HomeDeck 设备默认字体概念。

建议接口职责：

- 为 `M5Canvas` 加载或应用默认设备字体。
- 暴露加载成功与否，便于渲染器在失败时使用保底字体。
- 把字体资源数组与渲染器隔离，避免 `home_renderer.cpp` 和 `setup_renderer.cpp` 直接依赖生成文件细节。

`M5Canvas` 是每次渲染新建的 sprite，因此渲染器创建 canvas 后应通过统一入口应用默认设备字体。刷新频率较低，按渲染周期加载一次字体表可接受；实现时仍需确保 render 结束后 canvas 正常释放。

### 渲染器规则

首页和配网页设备屏幕以 Noto Sans SC 子集字体作为默认字体。业务代码不再把 `nullptr` 当作 HomeDeck 默认字体。

`HomeRenderer` 中的时间、日期、农历、节气、节假日、温湿度、日程和状态栏默认使用设备字体。温度里的 `°C` 不再拆成不同字体，只在必要时调整字号和基线。

`SetupRenderer` 中的标题、步骤、SSID、IP 地址默认使用设备字体。二维码绘制逻辑保持不变。

如字体加载失败，设备端允许回退到现有内置字体以保证屏幕仍可显示内容；正常路径不使用回退字体。

## 数据流

```text
fonts/NotoSansSC-Regular.ttf
  -> 字体生成脚本
  -> GB2312 字表
  -> Noto Sans SC VLW 子集
  -> C/C++ 字节数组
  -> DeviceFont API
  -> HomeRenderer / SetupRenderer 的 M5Canvas
```

## 错误处理

- 字体生成脚本如果找不到源 TTF，应失败并说明路径。
- 字体生成脚本如果无法生成非空字表，应失败并列出缺失来源。
- 固件运行时如果字体加载失败，渲染器使用保底内置字体，并继续显示内容。
- 如果生成字体导致固件超过 app 分区上限，停止实现并报告体积数据，再讨论缩小字表、改 SPIFFS 或调整分区。

## 测试与验证

Native 测试需要更新字体假实现，引入设备默认字体状态，替代当前“中文字体”和“默认字体”混用断言。

重点验证：

- 首页主要文本默认使用设备字体。
- 配网页文本默认使用设备字体。
- 长中文节假日和日程仍按设备字体宽度截断，不越界。
- 温度字符串可以作为整体绘制，符号位置与指标框边界正常。
- 配网页文本仍位于二维码区域上方。
- `pio test -e native` 通过。
- `pio run -e m5stack-papercolor` 通过，且固件大小未超过 app 分区。
- 字体生成脚本重复运行产物稳定。

## 影响范围

预计修改范围：

- 新增字体生成脚本和生成产物。
- 新增设备字体模块。
- 修改 `src/home_renderer.cpp` 和 `src/setup_renderer.cpp` 的字体应用方式与字号校准。
- 修改 native fake M5Unified 字体状态和相关渲染测试。
- 不修改 `lib/homedeck_core/src/homedeck/setup_page.h` 中的网页样式。

## 决策

采用内嵌 GB2312 子集 VLW 字体作为设备默认字体。这个方案比继续使用 `efontCN_14` 更符合指定字体源，也比全量 TTF 或 SD/SPIFFS 外部加载更适合当前分区和刷机流程。
