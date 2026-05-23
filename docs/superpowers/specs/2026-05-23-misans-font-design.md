# MiSans 多字体源设计文档

## 背景

当前项目使用单一的 `NotoSansSC-Regular.ttf` 生成三种 VLW 嵌入式字体（kBody 14px、kMetricValue 28px、kTime 42px）。用户希望将时间、温度湿度、其他文本分别使用不同粗细的 MiSans 字体，以提升视觉层次感。

## 目标

- 时间显示使用 **MiSans-Heavy**（最粗，突出时间）
- 温度/湿度数值使用 **MiSans-Bold**（较粗，强调指标）
- 其他文本（日期、标签、状态等）使用 **MiSans-Medium**（常规，保证可读性）

## 方案

扩展现有 `tools/generate_device_font.py` 脚本，支持为每个 `device_font::Role` 指定独立的源 TTF 文件和字号。设备端代码完全不变。

## 架构

```
fonts/misans/MiSans-Heavy.ttf  ──┐
fonts/misans/MiSans-Bold.ttf   ──┼──►  generate_device_font.py  ──►  src/generated/device_font_vlw.h/.cpp
fonts/misans/MiSans-Medium.ttf ──┘         (扩展为多源)
                                                  │
                                     ┌────────────┼────────────┐
                                     ▼            ▼            ▼
                              kDeviceTime   kDeviceMetric   kDeviceFont
                              VLW (42px)    VLW (28px)      VLW (14px)
```

## 生成端改动详情

### 配置映射

| Role | 源 TTF | 字号 | 输出变量名 | 目标字形 |
|------|--------|------|-----------|---------|
| `kTime` | `fonts/misans/MiSans-Heavy.ttf` | 42px | `kDeviceTimeFontVlw` | 数字、冒号、减号等 |
| `kMetricValue` | `fonts/misans/MiSans-Bold.ttf` | 28px | `kDeviceMetricFontVlw` | 数字、度符号、百分号等 |
| `kBody` | `fonts/misans/MiSans-Medium.ttf` | 14px | `kDeviceFontVlw` | ASCII + GB2312 + 源码扫描字符 |

### 脚本改动点

1. **配置结构**：将原来单一的 `SOURCE_FONT` 和按角色分组的字号配置，改为每个角色独立配置 `(ttf_path, pixel_size)` 元组。
2. **生成流程**：每个角色独立调用 `font_to_vlw.cpp`，生成独立的 `.vlw` 临时文件。
3. **输出合并**：保持现有的 `src/generated/device_font_vlw.h` 和 `.cpp` 输出格式，变量名不变，仅内容来源改变。
4. **字形收集策略不变**：
   - Time/Metric 字体仍只包含 `"0123456789:-+.°℃C% "`
   - Body 字体仍包含 ASCII + GB2312 + EXTRA_TEXT + 源码扫描字符

### 设备端改动

**无改动**。`homedeck::device_font::Role` 枚举和 `apply()` / `applyDefault()` 函数逻辑完全不变。设备端只感知到 VLW 数据内容变化，不感知来源变化。

## 错误处理

- 若某个 TTF 文件不存在，脚本立即报错退出，避免生成不完整的数据。
- 若 `font_to_vlw.cpp` 处理某个字体失败，同样报错退出。
- 保持现有最小字形数校验（`MIN_GLYPH_COUNT = 6000`，仅对 Body 字体）。

## 验证方式

1. 运行 `python tools/generate_device_font.py`，确认脚本成功完成
2. 检查 `src/generated/device_font_vlw.cpp` 中三个数组的内容是否已更新
3. 编译项目（`pio run`），确认无编译错误
4. 烧录到设备，目视确认：
   - 时间数字更粗（Heavy）
   - 温度/湿度数字较粗（Bold）
   - 日期/标签等正常粗细（Medium）

## 回退方案

若视觉效果不满意，用户提到可以考虑从 TF 卡运行时加载 TTF。这属于后续独立任务，不在本次设计范围内。本次修改仅涉及生成脚本，不涉及设备端架构变更，因此回退非常简单：只需将脚本改回使用单一 TTF 重新生成即可。

## 影响范围

| 文件 | 改动类型 |
|------|---------|
| `tools/generate_device_font.py` | 修改：支持多源 TTF |
| `src/generated/device_font_vlw.h` | 重新生成（内容变化） |
| `src/generated/device_font_vlw.cpp` | 重新生成（内容变化） |
| `src/device_font.h/.cpp` | 无改动 |
| `src/home_renderer.cpp` | 无改动 |
| `src/setup_renderer.cpp` | 无改动 |
