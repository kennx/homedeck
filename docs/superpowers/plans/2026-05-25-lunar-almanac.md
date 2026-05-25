# Lunar Almanac Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the complete offline Chinese almanac home-screen data path for 1900-2100, backed by a generated LittleFS binary package and filled into the existing Figma-style renderer.

**Architecture:** Generation-time Python code locks `lunar_python==1.4.8`, converts every date into the fields used by the Figma screen, and writes `data/almanac.bin` as a compact binary package with a string table. Device code keeps the new `AlmanacProvider` isolated from `HomeRenderer`: the provider decodes one day from LittleFS, and `makeHomeCalendarData()` converts either a hit or a missing-data result into `HomeCalendarData`.

**Tech Stack:** PlatformIO, Arduino ESP32-S3, M5Unified/M5GFX, LittleFS, Unity native tests, Python 3, `lunar_python==1.4.8`.

---

## Scope And File Map

This plan implements the approved spec at `docs/superpowers/specs/2026-05-25-lunar-almanac-design.md`.

Files to create:

- `tools/requirements-almanac.txt`: generation-time Python dependency pin.
- `tools/generate_almanac_data.py`: binary package writer, `lunar-python` field mapper, CLI, and golden checks.
- `tools/test_generate_almanac_data.py`: Python unit tests for binary packing and golden mapping.
- `src/almanac_provider.h`: public device-side almanac data structs and lookup API.
- `src/almanac_provider.cpp`: LittleFS binary reader, header validation, date offset calculation, string lookup, record decoding.
- `test/native/support/almanac_fixture.h`: native helper that builds tiny in-memory `almanac.bin` fixtures matching the production format.
- `test/native/test_almanac_provider/test_main.cpp`: Unity tests for provider hit, missing file, bad magic, bad CRC, and date range behavior.
- `data/almanac.bin`: generated 1900-2100 LittleFS data package.

Files to modify:

- `test/native/support/fake_arduino/LittleFS.h`: add fake `File`, in-memory files, `open`, `read`, `seek`, `size`, and reset helpers.
- `src/home_renderer.cpp`: use `AlmanacProvider` inside `makeHomeCalendarData()` and keep `HomeRenderer` drawing unchanged.
- `test/native/test_home_renderer/test_main.cpp`: update data tests to expect real almanac data when the fixture exists and explicit missing placeholders when it does not.

Binary format locked for this implementation:

- Header is exactly 64 bytes.
- Magic is `HDALM001`.
- Format version is `2`.
- Date range is `1900-01-01` through `2100-12-31`, inclusive.
- Day count is `73414`.
- Payload CRC32 is computed over the full payload after the 64-byte header: `record offset table + records + string table`.
- Header layout after `dayCount`:
  - `uint8_t maxYiCount`.
  - `uint8_t maxJiCount`.
  - `uint16_t termCount`.
  - `uint32_t recordOffsetsOffset`.
  - `uint32_t recordsOffset`.
  - `uint32_t stringTableOffset`.
  - `uint32_t stringCount`.
  - `uint32_t stringTableSize`.
  - `uint32_t payloadCrc32`.
  - `uint8_t recordOffsetSize`, fixed to `3`.
  - Remaining header bytes are reserved zero.
- Record offset table layout:
  - `(dayCount + 1)` 3-byte little-endian unsigned offsets.
  - Offsets are relative to `recordsOffset`.
  - Final offset equals the records blob length.
- Record layout is variable-size per day:
  - 11 little-endian `uint16_t` string indexes: lunar date, solar term, year ganzhi, month ganzhi, day ganzhi, day shengxiao, wuxing, chongsha, zhishen, jianchu, taishen.
  - `uint8_t yiCount`.
  - `uint8_t jiCount`.
  - `yiCount` one-byte term IDs.
  - `jiCount` one-byte term IDs.
- String table layout:
  - `(stringCount + 1)` little-endian `uint32_t` offsets.
  - UTF-8 string blob with no null terminators.
  - index `0` is always the empty string.
  - indexes `1..termCount` are the fixed yi/ji term dictionary; term ID `0` maps to string index `1`.

Figma date note: `2026-12-21` is used as a golden date because it appears in the Figma frame, but its expected almanac text comes from `lunar-python`; the mock text in the design frame is visual sample content and is not treated as data truth.

---

### Task 1: Generator Binary Format And Tests

**Files:**
- Create: `tools/test_generate_almanac_data.py`
- Create: `tools/generate_almanac_data.py`

This task is complete in the current branch and defines the executable Python source of truth for the v2 package format. Do not resurrect the earlier fixed-length daily entry template; later tasks must follow the compact format described at the top of this plan and in `tools/generate_almanac_data.py`.

- [x] **Step 1: Write Python tests for the v2 binary package format**

`tools/test_generate_almanac_data.py` must cover:

- Header magic `HDALM001`, format version `2`, 64-byte header, date range, day count, max yi/ji counts, term count, offset-table position, records position, string table position, string count, and `recordOffsetSize == 3`.
- Round-trip decoding of fixture days through `build_almanac_package()`, `unpack_header()`, `unpack_string_table()`, and `unpack_record()`.
- Raw little-endian layout for:
  - `(dayCount + 1)` 3-byte offsets starting at byte 64.
  - 11 `uint16_t` text indexes per daily entry.
  - `yiCount`, `jiCount`, then one-byte term IDs.
  - string table offsets with index `0` as the empty string and indexes `1..termCount` as the yi/ji term dictionary.
- CRC mutation coverage for the offset table, daily entry bytes, and string table bytes.

- [x] **Step 2: Add the v2 serializer and test helpers**

`tools/generate_almanac_data.py` must expose these package helpers:

- `AlmanacDay` and `AlmanacHeader` dataclasses matching the v2 header fields.
- 16-bit, 24-bit, and 32-bit little-endian pack/unpack helpers.
- `build_almanac_package(days)` that:
  - validates contiguous dates.
  - builds a yi/ji term dictionary first, preserving first-seen order.
  - places terms in string table indexes `1..termCount`, where term ID `0` maps to string index `1`.
  - splits `AlmanacDay.ganzhi` into year, month, day, and day-shengxiao components for storage.
  - stores 11 text fields as `uint16_t` string indexes.
  - stores yi/ji lists as one-byte term IDs.
  - writes the 3-byte offset table before the variable daily entries.
  - computes CRC32 over every byte after the header through the end of the string table.
  - raises `ValueError` if `termCount > 255` or `stringCount > 65535`.
- `unpack_header(data)`, `verify_payload_crc(data, header)`, `unpack_string_table(data, header)`, and `unpack_record(data, header, dayOffset, strings)` for tests and diagnostics.

- [x] **Step 3: Run the format tests**

Run:

```bash
python3 tools/test_generate_almanac_data.py
```

Expected: PASS.

---

### Task 2: Golden Source Mapping And Full Data Generation

**Files:**
- Create: `tools/requirements-almanac.txt`
- Modify: `tools/generate_almanac_data.py`
- Modify: `tools/test_generate_almanac_data.py`
- Create: `data/almanac.bin`

This task is complete in the current branch. Its output is the compact v2 LittleFS package; do not use old fixed-length entry templates in later work.

- [x] **Step 1: Pin and verify the generation dependency**

`tools/requirements-almanac.txt` must contain:

```text
lunar_python==1.4.8
```

`tools/generate_almanac_data.py` must define `REQUIRED_LUNAR_PYTHON_VERSION = "1.4.8"` and `_require_lunar_python()` must reject missing or mismatched versions with a venv-based install hint:

```bash
python3 -m venv .venv-almanac && . .venv-almanac/bin/activate && python3 -m pip install -r tools/requirements-almanac.txt
```

- [x] **Step 2: Add exact golden mapping tests**

Golden tests must cover:

- Figma date `2026-12-21`: `冬月十三`, empty solar term, `丙午年 庚子月 己巳日 蛇日`, and full yi/ji lists.
- Lunar new year `2026-02-17`: `正月初一`, expected ganzhi, expected taishen.
- Leap month `2025-07-25`: `闰六月初一`, expected ganzhi.
- Solar term day `2026-05-21`: `四月初五`, `小满`, `值神明堂`.
- Regular day `2026-03-03`: `正月十五`, `建除开日`, expected ji list.

`verify_golden()` must compare full `AlmanacDay` values against the shared golden expectations, not just check that fields are non-empty.

- [x] **Step 3: Generate the v2 LittleFS data package**

Run:

```bash
python3 tools/generate_almanac_data.py --verify-golden --output data/almanac.bin
```

Expected output includes:

```text
Generated data/almanac.bin
Date range: 1900-01-01..2100-12-31
Days: 73414
Record offset size: 3
Terms: 114
Strings: 992
Bytes: 3190030
```

The default `data/almanac.bin` output must stay under the LittleFS budget. Debug output to another path may be larger only if explicitly requested.

- [x] **Step 4: Verify package metadata and buildfs**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
import tools.generate_almanac_data as gen
p=Path('data/almanac.bin')
data=p.read_bytes()
h=gen.unpack_header(data)
print('bytes', len(data))
print(h)
print('crc', gen.verify_payload_crc(data, h))
assert len(data) < 0x360000
assert h.format_version == 2
assert h.day_count == gen.EXPECTED_DAY_COUNT
assert gen.verify_payload_crc(data, h)
PY
pio run -e m5stack-papercolor -t buildfs
git diff --check
```

Expected: all commands pass.

---

### Task 3: Native LittleFS Fake And V2 Almanac Provider Tests

**Files:**
- Modify: `test/native/support/fake_arduino/LittleFS.h`
- Create: `test/native/support/almanac_fixture.h`
- Create: `test/native/test_almanac_provider/test_main.cpp`

Task 3 must create tests and fixtures for the v2 compact package only. Do not copy any pre-v2 fixed-length entry code into native tests.

- [ ] **Step 1: Expand the LittleFS fake with file reads**

`test/native/support/fake_arduino/LittleFS.h` must provide:

- `FakeFile` with `read`, `seek`, `size`, `close`, and boolean conversion.
- `LittleFSClass::begin()`, `end()`, `open(path, mode)`, and `exists(path)`.
- In-memory file storage and helpers such as `fakeLittleFSReset()` and `fakeLittleFSSetFile(path, data)`.

Keep this fake scoped to the native tests. Do not touch production `src/` files in this task.

- [ ] **Step 2: Add a v2 native fixture package builder**

`test/native/support/almanac_fixture.h` must build small packages that match the current v2 Python generator:

- Header:
  - magic `HDALM001`.
  - format version `2`.
  - 64-byte header.
  - start/end dates and day count.
  - max yi/ji counts, term count, offset-table position, records position, string table position, string count, string table byte length, payload CRC32, and `recordOffsetSize == 3`.
- Payload:
  - `(dayCount + 1)` 3-byte offsets relative to the records start.
  - variable daily entries with 11 `uint16_t` text indexes, followed by counts and one-byte term IDs.
  - string table with index `0` empty, term strings in indexes `1..termCount`, then text strings.
- CRC:
  - compute CRC32 from byte 64 through the end of the string table.
  - bad CRC tests must fail if any byte in the offset table, daily entry area, or string table changes.
- Text packing:
  - split `ganzhi` into year, month, day, and day-shengxiao components before storing.
  - provider tests should expect these four components to be reconstructed into the original display string.

Useful fixture helpers:

- `AlmanacFixtureDay` with the same display fields as `AlmanacDayData`.
- `buildAlmanacFixturePackage(startYear, startMonth, startDay, days)`.
- `buildSingleDayFixturePackage()` using the existing `1900-01-01` fixture values.

- [ ] **Step 3: Write failing provider tests**

Create `test/native/test_almanac_provider/test_main.cpp` with native tests for:

- missing `/almanac.bin` returns false and balances `LittleFS.begin()` / `LittleFS.end()`.
- valid single-day v2 fixture decodes all display fields and joins yi/ji terms with spaces.
- dates outside the package range return false.
- bad magic returns false.
- bad format version returns false.
- bad payload CRC returns false.
- corrupted offset table returns false.
- invalid term ID returns false.

Run:

```bash
pio test -e native --filter native/test_almanac_provider
```

Expected at this task boundary: FAIL to compile because `src/almanac_provider.h` does not exist yet. Keep the failing tests for Task 4; do not commit them separately unless the implementation is also included.

---

### Task 4: Device-Side V2 Almanac Provider

**Files:**
- Create: `src/almanac_provider.h`
- Create: `src/almanac_provider.cpp`
- Test: `test/native/test_almanac_provider/test_main.cpp`

Task 4 must implement the native tests from Task 3 against the v2 compact package. Do not implement a reader for any older package layout.

- [ ] **Step 1: Add the provider header**

`src/almanac_provider.h` must define:

- `AlmanacDayData` with display strings: lunar date, solar term, ganzhi, wuxing, chongsha, zhishen, jianchu, taishen, yi, ji.
- `AlmanacProvider::lookup(int year, int month, int day, AlmanacDayData* out) const`.

- [ ] **Step 2: Add the provider implementation**

`src/almanac_provider.cpp` must include these v2 constants and helpers:

- path `/almanac.bin`.
- magic `HDALM001`.
- format version `2`.
- header byte length `64`.
- text field count `11`.
- offset width `3`.
- little-endian readers for 16-bit, 24-bit, and 32-bit unsigned values.
- CRC32 helper compatible with the Python generator.
- civil-date offset helper for range lookup.

Provider behavior requirements:

- Read and validate the full v2 header fields.
- Reject mismatched magic, format version, header length, offset width, zero string count, zero day count, out-of-range offsets, and payload regions outside the file size.
- Verify CRC32 over byte 64 through the end of the string table.
- Read the two adjacent 3-byte daily offsets to locate a variable entry.
- Decode 11 `uint16_t` text indexes in this order: lunar date, solar term, year ganzhi, month ganzhi, day ganzhi, day shengxiao, wuxing, chongsha, zhishen, jianchu, taishen.
- Reconstruct `ganzhi` as `年 月 日 生肖日`, matching generator round-trip tests.
- Decode yi/ji as one-byte term IDs, where term ID `0` maps to string index `1`.
- Reject term IDs outside `termCount` and string indexes outside `stringCount`.
- Join yi and ji terms with a single ASCII space for `AlmanacDayData`.
- Always close the file and call `LittleFS.end()` after lookup attempts.

- [ ] **Step 3: Run provider tests**

Run:

```bash
pio test -e native --filter native/test_almanac_provider
```

Expected: PASS.

- [ ] **Step 4: Run the full native suite**

Run:

```bash
pio test -e native
```

Expected: PASS.

- [ ] **Step 5: Commit the provider**

```bash
git add src/almanac_provider.h src/almanac_provider.cpp test/native/support/fake_arduino/LittleFS.h test/native/support/almanac_fixture.h test/native/test_almanac_provider/test_main.cpp
git commit -m "feat: 添加离线黄历数据读取器" \
  -m "- 扩展 LittleFS fake 支持内存文件读取" \
  -m "- 增加 v2 almanac.bin fixture 生成器和 provider native 测试" \
  -m "- 校验 HDALM001 v2 header、payload CRC 和变长记录解码"
```

---

### Task 5: Home Calendar Data Integration

**Files:**
- Modify: `src/home_renderer.cpp`
- Modify: `test/native/test_home_renderer/test_main.cpp`
- Test: `test/native/test_home_renderer/test_main.cpp`

- [ ] **Step 1: Update home data tests for almanac hit and missing fallback**

Modify `test/native/test_home_renderer/test_main.cpp`:

1. Add includes near the top:

```cpp
#include <LittleFS.h>

#include "almanac_fixture.h"
```

2. Update `setUp()` to reset LittleFS:

```cpp
void setUp() {
  M5 = FakeM5Global{};
  gLastQrCodeText.clear();
  fakeLittleFSReset();
}
```

3. Replace `test_home_calendar_data_uses_supplied_local_date()` with:

```cpp
void test_home_calendar_data_uses_supplied_local_date_and_almanac_fixture() {
  fakeLittleFSSetFile("/almanac.bin", homedeck::test::buildSingleDayFixturePackage());
  std::tm local{};
  local.tm_year = 1900 - 1900;
  local.tm_mon = 0;
  local.tm_mday = 1;
  local.tm_wday = 1;

  const auto data = homedeck::makeHomeCalendarData(local);

  TEST_ASSERT_EQUAL_STRING("1900 年", data.year.c_str());
  TEST_ASSERT_EQUAL_STRING("一月", data.month.c_str());
  TEST_ASSERT_EQUAL_STRING("1", data.day.c_str());
  TEST_ASSERT_EQUAL_STRING("星期一", data.weekday.c_str());
  TEST_ASSERT_FALSE(data.isHoliday);
  TEST_ASSERT_EQUAL_STRING("腊月初一", data.lunarDate.c_str());
  TEST_ASSERT_EQUAL_STRING("", data.solarTerm.c_str());
  TEST_ASSERT_EQUAL_STRING("己亥年 丙子月 甲子日 鼠日", data.ganzhi.c_str());
  TEST_ASSERT_EQUAL_STRING("五行海中金", data.wuxing.c_str());
  TEST_ASSERT_EQUAL_STRING("冲马煞南", data.chongsha.c_str());
  TEST_ASSERT_EQUAL_STRING("值神青龙", data.zhishen.c_str());
  TEST_ASSERT_EQUAL_STRING("建除建日", data.jianchu.c_str());
  TEST_ASSERT_EQUAL_STRING("胎神占门碓外东南", data.taishen.c_str());
  TEST_ASSERT_EQUAL_STRING("祭祀 祈福", data.yi.c_str());
  TEST_ASSERT_EQUAL_STRING("嫁娶", data.ji.c_str());
}
```

4. Add a new fallback test:

```cpp
void test_home_calendar_data_uses_missing_placeholders_when_almanac_file_is_absent() {
  std::tm local{};
  local.tm_year = 2030 - 1900;
  local.tm_mon = 8;
  local.tm_mday = 8;
  local.tm_wday = 0;

  const auto data = homedeck::makeHomeCalendarData(local);

  TEST_ASSERT_EQUAL_STRING("2030 年", data.year.c_str());
  TEST_ASSERT_EQUAL_STRING("九月", data.month.c_str());
  TEST_ASSERT_EQUAL_STRING("8", data.day.c_str());
  TEST_ASSERT_EQUAL_STRING("星期日", data.weekday.c_str());
  TEST_ASSERT_TRUE(data.isHoliday);
  TEST_ASSERT_EQUAL_STRING("数据缺失", data.lunarDate.c_str());
  TEST_ASSERT_EQUAL_STRING("", data.solarTerm.c_str());
  TEST_ASSERT_EQUAL_STRING("黄历数据缺失", data.ganzhi.c_str());
  TEST_ASSERT_EQUAL_STRING("五行暂无", data.wuxing.c_str());
  TEST_ASSERT_EQUAL_STRING("冲煞暂无", data.chongsha.c_str());
  TEST_ASSERT_EQUAL_STRING("值神暂无", data.zhishen.c_str());
  TEST_ASSERT_EQUAL_STRING("建除暂无", data.jianchu.c_str());
  TEST_ASSERT_EQUAL_STRING("胎神暂无", data.taishen.c_str());
  TEST_ASSERT_EQUAL_STRING("暂无", data.yi.c_str());
  TEST_ASSERT_EQUAL_STRING("暂无", data.ji.c_str());
}
```

5. Update `main()` registrations:

```cpp
  RUN_TEST(test_home_calendar_data_uses_supplied_local_date_and_almanac_fixture);
  RUN_TEST(test_home_calendar_data_uses_missing_placeholders_when_almanac_file_is_absent);
```

- [ ] **Step 2: Run home renderer tests to verify they fail**

Run:

```bash
pio test -e native --filter native/test_home_renderer
```

Expected: FAIL because `makeHomeCalendarData()` still emits placeholder almanac fields.

- [ ] **Step 3: Integrate `AlmanacProvider` into `makeHomeCalendarData()`**

Modify `src/home_renderer.cpp`:

1. Add the include after existing local includes:

```cpp
#include "almanac_provider.h"
```

2. Add helper functions inside the anonymous namespace after `fallbackLocalTime()`:

```cpp
void applyMissingAlmanac(HomeCalendarData& data) {
  data.lunarDate = "数据缺失";
  data.solarTerm = "";
  data.ganzhi = "黄历数据缺失";
  data.wuxing = "五行暂无";
  data.chongsha = "冲煞暂无";
  data.zhishen = "值神暂无";
  data.jianchu = "建除暂无";
  data.taishen = "胎神暂无";
  data.yi = "暂无";
  data.ji = "暂无";
}

void applyAlmanac(HomeCalendarData& data, const AlmanacDayData& almanac) {
  data.lunarDate = almanac.lunarDate;
  data.solarTerm = almanac.solarTerm;
  data.ganzhi = almanac.ganzhi;
  data.wuxing = almanac.wuxing;
  data.chongsha = almanac.chongsha;
  data.zhishen = almanac.zhishen;
  data.jianchu = almanac.jianchu;
  data.taishen = almanac.taishen;
  data.yi = almanac.yi.empty() ? "暂无" : almanac.yi;
  data.ji = almanac.ji.empty() ? "暂无" : almanac.ji;
}
```

3. Replace the almanac placeholder assignments in `makeHomeCalendarData()` with provider lookup:

```cpp
  AlmanacProvider provider;
  AlmanacDayData almanac{};
  if (provider.lookup(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday, &almanac)) {
    applyAlmanac(data, almanac);
  } else {
    applyMissingAlmanac(data);
  }
```

The beginning of `makeHomeCalendarData()` remains:

```cpp
HomeCalendarData makeHomeCalendarData(const std::tm& localTime) {
  const int weekday = localTime.tm_wday;
  HomeCalendarData data{};
  data.year = formatYear(localTime.tm_year + 1900);
  data.month = chineseMonthName(localTime.tm_mon);
  data.day = formatDay(localTime.tm_mday);
  data.weekday = weekdayName(weekday);
  data.isHoliday = weekday == 0 || weekday == 6;
```

- [ ] **Step 4: Run home renderer tests**

Run:

```bash
pio test -e native --filter native/test_home_renderer
```

Expected: PASS.

- [ ] **Step 5: Run provider and home renderer tests together**

Run:

```bash
pio test -e native --filter native/test_almanac_provider
pio test -e native --filter native/test_home_renderer
```

Expected: PASS.

- [ ] **Step 6: Commit home integration**

```bash
git add src/home_renderer.cpp test/native/test_home_renderer/test_main.cpp
git commit -m "feat: 首页接入离线黄历数据" \
  -m "- makeHomeCalendarData 从 AlmanacProvider 填充真实黄历字段" \
  -m "- 数据缺失时保留公历并显示明确占位" \
  -m "- 扩展首页 native 测试覆盖命中与降级路径"
```

---

### Task 6: Full Verification And BuildFS

**Files:**
- Verify: `data/almanac.bin`
- Verify: `src/almanac_provider.cpp`
- Verify: `src/home_renderer.cpp`
- Verify: `test/native/**`

- [ ] **Step 1: Regenerate the almanac package**

Run:

```bash
python3 tools/generate_almanac_data.py --verify-golden --output data/almanac.bin
```

Expected: exits 0 and prints `Days: 73414`.

- [ ] **Step 2: Run Python generator tests**

Run:

```bash
python3 tools/test_generate_almanac_data.py
```

Expected: PASS with `Ran 4 tests`.

- [ ] **Step 3: Run native tests**

Run:

```bash
pio test -e native
```

Expected: PASS.

- [ ] **Step 4: Build device firmware**

Run:

```bash
pio run -e m5stack-papercolor
```

Expected: SUCCESS.

- [ ] **Step 5: Build LittleFS image**

Run:

```bash
pio run -e m5stack-papercolor -t buildfs
```

Expected: SUCCESS and the LittleFS image includes `almanac.bin`.

- [ ] **Step 6: Check for whitespace and generated-data drift**

Run:

```bash
git diff --check
git status --short
```

Expected: `git diff --check` exits 0. `git status --short` shows only intentional files if the previous tasks have not committed final regeneration output.

- [ ] **Step 7: Commit final regenerated data if needed**

If `git status --short` shows only `data/almanac.bin` changed after regeneration, commit it:

```bash
git add data/almanac.bin
git commit -m "chore: 刷新离线黄历数据包" \
  -m "- 使用锁定的 lunar_python==1.4.8 重新生成 almanac.bin" \
  -m "- 保持 1900-2100 数据范围和 golden 校验一致"
```

If `git status --short` is clean, do not create an empty commit.
