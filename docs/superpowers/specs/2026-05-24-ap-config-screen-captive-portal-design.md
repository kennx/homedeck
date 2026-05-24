# AP 配置屏幕与 Captive Portal 设计

## 背景

HomeDeck 进入 AP 配置模式后，需要在 PaperColor 400x600 竖屏上显示清晰的配置入口：Logo、AP 名称、IP 地址和二维码。二维码用于连接设备 AP；连接成功后，设备侧通过 captive portal/DNS 兜底把用户更稳定地带到 `192.168.4.1` 配置页。

本设计只覆盖 AP 配置入口屏、启动首页 Logo、设备字体/图片资源和 captive portal 入口能力。不重写配置表单 UI、Wi-Fi 保存逻辑或启动模式状态机。

## 目标

- AP 配置屏显示四个元素，全部水平居中：
  - Logo：使用 `assets/images/logo.png`，顶部距离屏幕 86px。
  - AP 名称：`HomeDeck-XXXX`，MiSans SemiBold 20px，位于 Logo 下方，和 Logo 底部间距 26px。
  - IP 地址：`192.168.4.1`，MiSans SemiBold 20px，位于 AP 名称下方，和 AP 文本底部间距 26px。
  - 二维码：256x256，位于 IP 地址下方，和 IP 文本底部间距 26px。
- 普通启动后的 HomeDeck 首页改为白底居中 Logo，不再绘制 `HomeDeck` 文本。
- 二维码只放一个，内容用于连接 AP：`WIFI:T:nopass;S:HomeDeck-XXXX;;`。
- 设备 AP 侧增加轻量 captive portal/DNS 重定向，让用户连接 AP 后更稳定地进入 `192.168.4.1`。

## 非目标

- 不修改现有配置表单字段、保存流程和校验规则。
- 不新增多二维码、引导文案或额外屏幕状态。
- 不扩大字体字符集到完整 Unicode。
- 不改首页业务数据渲染逻辑；本次只把当前启动首页占位替换成居中 Logo。

## 架构

### 屏幕渲染

`HomeRenderer` 继续作为设备屏幕渲染入口：

- `render()`：白底、竖屏、绘制居中 Logo。
- `renderConfigPortal(apSsid, ipAddress)`：白底、竖屏，按 Figma 节点 `7:194` 的 400x600 布局绘制 Logo、AP 名称、IP 地址和二维码。

渲染层只接收 AP 名称和 IP 地址，不负责启动 AP、处理 HTTP 或 DNS。

### 资源

Logo 源文件为 `assets/images/logo.png`。实现时按 PlatformIO 文件系统方式放置为 `data/logo.png`，并在 `platformio.ini` 中启用 LittleFS。运行时使用 `LittleFS` 读取 `/logo.png` 并通过 M5GFX PNG 绘制接口显示。

MiSans SemiBold 20px 通过 `tools/generate_device_font.py` 生成 VLW 资源，新增配置屏专用字体符号 `kConfigPortalFontVlw`。生成器同时修正 body 字体来源为现有的 `fonts/misans/MiSans-Semibold.ttf`，并继续从 VLW header 读取真实 glyph count，避免 metadata 和实际资源漂移。

### Captive Portal

`ConfigPortal` 保持现有 WebServer 入口，并新增 DNS 处理：

- `begin()` 启动 AP 后，启动 `DNSServer` 监听 53 端口。
- DNS 使用通配域名解析，统一返回 `WiFi.softAPIP()`。
- `handleClient()` 同时调用现有 `server_.handleClient()` 和新增的 `dnsServer_.processNextRequest()`。
- HTTP 侧保留 `/` 与 `/save`，并为常见 captive portal 探测路径返回重定向或配置页入口，把用户带到 `/`。

这让扫码后的第一动作仍是连接 AP；连接后，系统探测或用户打开任意域名时更容易落到 `http://192.168.4.1/`。

## 布局细节

屏幕尺寸按 PaperColor 竖屏 400x600：

| 元素 | 尺寸/字体 | X | Y/间距 |
| --- | --- | --- | --- |
| Logo | 约 297x40 | 居中 | 顶部 86px |
| AP 名称 | MiSans SemiBold 20px | 居中 | Logo 底部下方 26px |
| IP 地址 | MiSans SemiBold 20px | 居中 | AP 文本底部下方 26px |
| 二维码 | 256x256 | 居中 | IP 文本底部下方 26px |

Figma metadata 中对应节点为：

- Frame：`7:194`，400x600。
- Logo：`9:214`，x 约 51.54，y 约 86.37，尺寸约 296.92x39.26。
- AP 名称：`9:199`，x 50，y 约 151.63，尺寸 300x27。
- IP 地址：`9:213`，x 50，y 约 204.63，尺寸 300x27。
- 二维码占位：`9:198`，x 72，y 约 257.63，尺寸 256x256。

实现中优先使用元素左上角和已知尺寸计算中心点，Logo 顶部按 86px 落地，其他 Figma 小数坐标按最接近整数计算，避免混淆 datum 后造成间距偏移。

## 数据流

1. `BootController` 进入配置模式。
2. `AppRuntime` 生成 `HomeDeck-XXXX` AP 名称，启动 `ConfigPortal`。
3. `ConfigPortal` 开启 SoftAP、WebServer 和 DNS 重定向。
4. `HomeRenderer::renderConfigPortal()` 绘制入口屏：
   - 从 LittleFS 绘制 `/logo.png`。
   - 加载 MiSans SemiBold 20px VLW 字体。
   - 绘制 AP 名称和 IP 地址。
   - 使用 QRCode 库生成并绘制 Wi-Fi 二维码。
5. 用户扫码连接 AP。
6. 手机系统 captive portal 探测或浏览器访问被 DNS/HTTP 兜底引导到配置页。

## 错误处理

- Logo 文件系统挂载或 PNG 绘制失败时，不阻断配置门户启动；屏幕仍显示 AP、IP 和二维码。
- 配置字体加载失败时回退默认字体，并保持文本居中。
- DNS 启动失败时仍保留现有 Web 配置页，用户可手动访问 `192.168.4.1`。
- 二维码模块尺寸按目标 256px 计算，实际绘制尺寸取整后保持居中，并通过测试确保不越界。

## 测试策略

### Native 测试

- 更新 `test_home_renderer`：
  - `render()` 只绘制居中 Logo，白底竖屏。
  - `renderConfigPortal()` 绘制 `/logo.png`，坐标居中。
  - AP 名称为 `HomeDeck-XXXX`，不加 `AP:` 前缀。
  - IP 地址为 `192.168.4.1`，不加 `IP:` 前缀。
  - AP/IP 使用配置屏 VLW 字体并水平居中。
  - 二维码内容为 `WIFI:T:nopass;S:HomeDeck-XXXX;;`。
  - 二维码绘制区域在 400x600 内。
- 扩展 fake M5GFX：
  - 记录 `drawPngFile` 路径、坐标和 datum。
  - 记录配置屏字体加载和卸载。
  - 记录二维码模块绘制矩形。
- 扩展 fake QRCode：
  - 记录 `qrcode_initText()` 接收到的二维码内容。
- 为 `ConfigPortal` 增加或扩展测试：
  - `begin()` 启动 DNS。
  - `handleClient()` 同时处理 HTTP 和 DNS。
  - 常见探测路径返回重定向或配置页入口。

### 验收命令

```bash
python3 tools/generate_device_font.py
pio test -e native --filter native/test_home_renderer
pio test -e native
pio run -e m5stack-papercolor
git diff --check
```

## 实施顺序

1. 修正字体生成脚本并生成 MiSans SemiBold 20px 配置屏 VLW。
2. 添加 LittleFS logo 资源路径和 PlatformIO 文件系统配置。
3. 扩展 fake M5GFX、fake QRCode 和必要的 fake DNS/WebServer 能力。
4. 先更新 native 测试表达目标布局和 captive portal 行为。
5. 实现 `HomeRenderer` 新布局和 Logo 首页。
6. 实现 `ConfigPortal` DNS/HTTP captive portal 兜底。
7. 跑完整验收命令，确认生成文件稳定、native 测试和固件构建通过。
