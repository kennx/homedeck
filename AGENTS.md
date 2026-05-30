# Agent Guide

这是 HomeDeck 固件项目的根级 Agent 指南。保持简洁，只保留高频规则：项目地图、硬约束和工作流要求 —— 每个任务都需要知道的事情。

## Working Principles

- 从第一性原理思考。从真实需求、代码事实和验证结果出发；如果目标不清晰，先与用户讨论。
- 把代码而非文档当作真相来源。除非用户明确指示，不要为了理解实现而去阅读普通 Markdown 文档。
- 修改代码前，先读取相关代码和最近的约束，并遵循目录树中最近的 `AGENTS.md`。
- 保持改动聚焦。不要顺带塞进无关的重构。

## Project Map

```
src/
├── app/              # 应用生命周期与协调
├── views/            # 所有视图渲染器
├── system/           # 硬件交互与系统服务
├── config/           # 配置与配网
├── providers/        # 数据/内容提供者
└── generated/        # 工具生成文件（保持现状）

test/native/          # 单元测试（Unity），每个被测模块对应一个 test_<module>/
tools/                # 构建时 Python 脚本（字体/老黄历数据生成）
data/                 # 运行时资源（logo.png、almanac.bin），打包到 LittleFS
docs/                 # 产品文档和开发记录 — 硬件信息见 [docs/PaperColor.md](docs/PaperColor.md)
fonts/                # 字体源文件，经 tools/ 生成 src/generated/
```

## Environment Requirements

- **PlatformIO Core**：用于构建、烧录和测试（`pio run`、`pio test`）。
- **Python 3**：运行 `tools/` 下的生成脚本。
- **目标平台**：`espressif32 @ 6.12.0`，`board = esp32-s3-devkitc-1`，`framework = arduino`。
- **目标硬件**：M5Stack PaperColor（ESP32-S3 + 电子墨水屏）。
- **核心库**：M5Unified、M5GFX、M5PM1。
- 固件使用 `gnu++17`；native 测试同样使用 `gnu++17`。

## General Coding Rules

- 所有业务代码放在 `homedeck` namespace 下。
- 优先使用依赖注入：通过 `XxxDeps` 结构体将外部依赖（硬件、IO、时间）传入构造函数，便于在 native 测试中注入 mock。
- 头文件统一使用 `#pragma once`。
- 类成员变量使用 trailing underscore 命名（如 `deps_`、`currentView_`）。
- 枚举使用 `enum class`。
- 不要在头文件中引入不必要的 Arduino / M5 头文件，保持头文件轻量；实现文件再包含具体平台头。
- 不要添加过多新的测试文件。优先把测试加到对应模块已有的 `test_main.cpp` 中。
- 当测试因用户修改而失败时，默认先修复测试；除非实现确实有 bug，否则不要为了迁就旧测试而改动实现。
- 不要为了外部兼容性牺牲代码质量，除非用户明确要求。破坏性改动需要用户确认。

## Where to Update Instructions

- 影响几乎所有任务的硬规则：更新根目录的 `AGENTS.md`。
- 只影响特定目录的规则：更新该目录下最近的 `AGENTS.md`。
- 保持指令更新聚焦，并以代码事实为支撑。

## Workflow Requirements

- 如果 `rg` / `rg --files` 可以使用，读取代码时优先使用
- 设计改动时，先遵循现有的边界和局部模式。
- 提交代码前，确保 native 测试通过：
  ```bash
  pio test -e native
  ```
- **git commit 规范**：
  - 使用中文。
  - 标题行使用 conventional commits 格式（`feat:` / `fix:` / `refactor:` / `chore:` 等）。
  - body 中按文件或功能分组，说明改了什么、为什么改、影响范围。
  - 修复 bug 需说明根因；架构决策需简要说明理由。

## 查询与验证顺序

遇到不确定时：

1. 能否用 **context7** 查 M5Unified / M5GFX / M5PM1 官方文档？
   - 是 → 查文档，获取准确信息。
2. 能否用 **GitHub MCP** 阅读相关开源库源码、Issues 或 Release Notes？
   - 是 → 直接阅读仓库源码或历史 issue，验证 API 行为、已知 bug、版本兼容性。
3. 能否用 **tavily** / **searxng** 搜索社区方案？
   - 是 → 搜索并交叉验证 2–3 个来源。
4. 否 / 搜索结果矛盾 → 停下来，向用户说明困惑点，请求澄清。

## 绝不编造的规则

- **不编造 API 参数**：如果不确定 `M5.Power.getBatteryVoltage()` 的返回值类型，用 context7 查 M5Unified 文档。
- **不编造引脚定义**：如果不确定硬件接口对应哪个 GPIO，用 context7 查 Cardputer-Adv PinMap。
- **不编造硬件限制**：如果不确定 ESP32-S3 的某项能力，搜索官方 specs。
- **不编造库版本**：如果不确定某个库是否支持特定功能，查库文档或 GitHub release notes。
