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

- [ ] **Step 1: Write failing Python tests for the binary package format**

Create `tools/test_generate_almanac_data.py`:

```python
#!/usr/bin/env python3
from __future__ import annotations

import sys
import unittest
from datetime import date
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

import generate_almanac_data as gen  # noqa: E402


class AlmanacPackageFormatTest(unittest.TestCase):
    def test_fixture_package_has_expected_header_and_strings(self) -> None:
        days = [
            gen.AlmanacDay(
                solar_date=date(1900, 1, 1),
                lunar_date="腊月初一",
                solar_term="",
                ganzhi="己亥年 丙子月 甲子日 鼠日",
                wuxing="五行海中金",
                chongsha="冲马煞南",
                zhishen="值神青龙",
                jianchu="建除建日",
                taishen="胎神占门碓外东南",
                yi=("祭祀", "祈福"),
                ji=("嫁娶",),
            ),
            gen.AlmanacDay(
                solar_date=date(1900, 1, 2),
                lunar_date="腊月初二",
                solar_term="",
                ganzhi="己亥年 丙子月 乙丑日 牛日",
                wuxing="五行海中金",
                chongsha="冲羊煞东",
                zhishen="值神明堂",
                jianchu="建除除日",
                taishen="胎神碓磨厕外东南",
                yi=("出行",),
                ji=("安葬", "开市"),
            ),
        ]

        package = gen.build_almanac_package(days)
        header = gen.unpack_header(package)
        strings = gen.unpack_string_table(package, header)
        first = gen.unpack_record(package, header, 0, strings)
        second = gen.unpack_record(package, header, 1, strings)

        self.assertEqual(gen.MAGIC, header.magic)
        self.assertEqual(1, header.format_version)
        self.assertEqual(64, header.header_size)
        self.assertEqual(date(1900, 1, 1), header.start_date)
        self.assertEqual(date(1900, 1, 2), header.end_date)
        self.assertEqual(2, header.day_count)
        self.assertEqual(2, header.max_yi_count)
        self.assertEqual(2, header.max_ji_count)
        self.assertEqual(64, header.records_offset)
        self.assertGreater(header.string_table_offset, header.records_offset)
        self.assertEqual("", strings[0])
        self.assertEqual(days[0], first)
        self.assertEqual(days[1], second)

    def test_crc_changes_when_payload_is_modified(self) -> None:
        day = gen.AlmanacDay(
            solar_date=date(1900, 1, 1),
            lunar_date="腊月初一",
            solar_term="",
            ganzhi="己亥年 丙子月 甲子日 鼠日",
            wuxing="五行海中金",
            chongsha="冲马煞南",
            zhishen="值神青龙",
            jianchu="建除建日",
            taishen="胎神占门碓外东南",
            yi=("祭祀",),
            ji=("嫁娶",),
        )
        package = bytearray(gen.build_almanac_package([day]))
        header = gen.unpack_header(package)
        self.assertTrue(gen.verify_payload_crc(package, header))

        package[-1] ^= 0x01
        self.assertFalse(gen.verify_payload_crc(package, header))


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```bash
python3 tools/test_generate_almanac_data.py
```

Expected: FAIL with `ModuleNotFoundError: No module named 'generate_almanac_data'`.

- [ ] **Step 3: Add the binary serializer and test helpers**

Create `tools/generate_almanac_data.py` with this initial implementation:

```python
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import struct
import zlib
from dataclasses import dataclass
from datetime import date, timedelta
from pathlib import Path
from typing import Iterable, Sequence


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "data" / "almanac.bin"
MAGIC = b"HDALM001"
FORMAT_VERSION = 1
HEADER_SIZE = 64
START_DATE = date(1900, 1, 1)
END_DATE = date(2100, 12, 31)
EXPECTED_DAY_COUNT = 73414
TEXT_FIELD_COUNT = 8


@dataclass(frozen=True)
class AlmanacDay:
    solar_date: date
    lunar_date: str
    solar_term: str
    ganzhi: str
    wuxing: str
    chongsha: str
    zhishen: str
    jianchu: str
    taishen: str
    yi: tuple[str, ...]
    ji: tuple[str, ...]


@dataclass(frozen=True)
class AlmanacHeader:
    magic: bytes
    format_version: int
    header_size: int
    start_date: date
    end_date: date
    day_count: int
    record_size: int
    max_yi_count: int
    max_ji_count: int
    records_offset: int
    string_table_offset: int
    string_count: int
    string_table_size: int
    payload_crc32: int


def _pack_u16(value: int) -> bytes:
    return struct.pack("<H", value)


def _pack_i16(value: int) -> bytes:
    return struct.pack("<h", value)


def _pack_u32(value: int) -> bytes:
    return struct.pack("<I", value)


def _unpack_u16(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def _unpack_i16(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<h", data, offset)[0]


def _unpack_u32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def _date_bytes(value: date) -> bytes:
    return _pack_i16(value.year) + bytes((value.month, value.day))


def _read_date(data: bytes | bytearray, offset: int) -> date:
    return date(_unpack_i16(data, offset), data[offset + 2], data[offset + 3])


def _text_fields(day: AlmanacDay) -> tuple[str, ...]:
    return (
        day.lunar_date,
        day.solar_term,
        day.ganzhi,
        day.wuxing,
        day.chongsha,
        day.zhishen,
        day.jianchu,
        day.taishen,
    )


def build_string_table(days: Sequence[AlmanacDay]) -> tuple[dict[str, int], bytes, int]:
    strings: list[str] = [""]
    seen: dict[str, int] = {"": 0}
    for day in days:
        for value in (*_text_fields(day), *day.yi, *day.ji):
            if value not in seen:
                seen[value] = len(strings)
                strings.append(value)

    offsets: list[int] = []
    blob = bytearray()
    for value in strings:
        offsets.append(len(blob))
        blob.extend(value.encode("utf-8"))
    offsets.append(len(blob))

    table = bytearray()
    for offset in offsets:
        table.extend(_pack_u32(offset))
    table.extend(blob)
    return seen, bytes(table), len(strings)


def build_records(days: Sequence[AlmanacDay], string_index: dict[str, int]) -> tuple[bytes, int, int, int]:
    max_yi = max((len(day.yi) for day in days), default=0)
    max_ji = max((len(day.ji) for day in days), default=0)
    if max_yi > 255 or max_ji > 255:
        raise ValueError(f"yi/ji count exceeds uint8 limit: yi={max_yi}, ji={max_ji}")

    record_size = 36 + (max_yi + max_ji) * 4
    records = bytearray()
    for day in days:
        for value in _text_fields(day):
            records.extend(_pack_u32(string_index[value]))
        records.extend(bytes((len(day.yi), len(day.ji))))
        records.extend(_pack_u16(0))
        for value in day.yi:
            records.extend(_pack_u32(string_index[value]))
        for _ in range(max_yi - len(day.yi)):
            records.extend(_pack_u32(0))
        for value in day.ji:
            records.extend(_pack_u32(string_index[value]))
        for _ in range(max_ji - len(day.ji)):
            records.extend(_pack_u32(0))

    return bytes(records), record_size, max_yi, max_ji


def build_header(
    *,
    start_date: date,
    end_date: date,
    day_count: int,
    record_size: int,
    max_yi_count: int,
    max_ji_count: int,
    records_offset: int,
    string_table_offset: int,
    string_count: int,
    string_table_size: int,
    payload_crc32: int,
) -> bytes:
    header = bytearray()
    header.extend(MAGIC)
    header.extend(_pack_u16(FORMAT_VERSION))
    header.extend(_pack_u16(HEADER_SIZE))
    header.extend(_date_bytes(start_date))
    header.extend(_date_bytes(end_date))
    header.extend(_pack_u32(day_count))
    header.extend(_pack_u16(record_size))
    header.extend(bytes((max_yi_count, max_ji_count)))
    header.extend(_pack_u32(records_offset))
    header.extend(_pack_u32(string_table_offset))
    header.extend(_pack_u32(string_count))
    header.extend(_pack_u32(string_table_size))
    header.extend(_pack_u32(payload_crc32))
    header.extend(bytes(HEADER_SIZE - len(header)))
    if len(header) != HEADER_SIZE:
        raise ValueError(f"header size mismatch: {len(header)}")
    return bytes(header)


def build_almanac_package(days: Sequence[AlmanacDay]) -> bytes:
    if not days:
        raise ValueError("at least one almanac day is required")
    for index in range(1, len(days)):
        expected = days[index - 1].solar_date + timedelta(days=1)
        if days[index].solar_date != expected:
            raise ValueError(f"non-contiguous date at index {index}: {days[index].solar_date}")

    string_index, string_table, string_count = build_string_table(days)
    records, record_size, max_yi, max_ji = build_records(days, string_index)
    records_offset = HEADER_SIZE
    string_table_offset = records_offset + len(records)
    payload = records + string_table
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    header = build_header(
        start_date=days[0].solar_date,
        end_date=days[-1].solar_date,
        day_count=len(days),
        record_size=record_size,
        max_yi_count=max_yi,
        max_ji_count=max_ji,
        records_offset=records_offset,
        string_table_offset=string_table_offset,
        string_count=string_count,
        string_table_size=len(string_table),
        payload_crc32=crc,
    )
    return header + payload


def unpack_header(data: bytes | bytearray) -> AlmanacHeader:
    if len(data) < HEADER_SIZE:
        raise ValueError("data is shorter than header")
    return AlmanacHeader(
        magic=bytes(data[0:8]),
        format_version=_unpack_u16(data, 8),
        header_size=_unpack_u16(data, 10),
        start_date=_read_date(data, 12),
        end_date=_read_date(data, 16),
        day_count=_unpack_u32(data, 20),
        record_size=_unpack_u16(data, 24),
        max_yi_count=data[26],
        max_ji_count=data[27],
        records_offset=_unpack_u32(data, 28),
        string_table_offset=_unpack_u32(data, 32),
        string_count=_unpack_u32(data, 36),
        string_table_size=_unpack_u32(data, 40),
        payload_crc32=_unpack_u32(data, 44),
    )


def verify_payload_crc(data: bytes | bytearray, header: AlmanacHeader) -> bool:
    payload = data[header.records_offset : header.string_table_offset + header.string_table_size]
    return (zlib.crc32(payload) & 0xFFFFFFFF) == header.payload_crc32


def unpack_string_table(data: bytes | bytearray, header: AlmanacHeader) -> list[str]:
    base = header.string_table_offset
    offsets = [_unpack_u32(data, base + index * 4) for index in range(header.string_count + 1)]
    blob_start = base + (header.string_count + 1) * 4
    strings: list[str] = []
    for index in range(header.string_count):
        start = blob_start + offsets[index]
        end = blob_start + offsets[index + 1]
        strings.append(bytes(data[start:end]).decode("utf-8"))
    return strings


def unpack_record(
    data: bytes | bytearray,
    header: AlmanacHeader,
    day_offset: int,
    strings: Sequence[str],
) -> AlmanacDay:
    if day_offset < 0 or day_offset >= header.day_count:
        raise IndexError(day_offset)
    offset = header.records_offset + day_offset * header.record_size
    fields = [strings[_unpack_u32(data, offset + index * 4)] for index in range(TEXT_FIELD_COUNT)]
    count_offset = offset + TEXT_FIELD_COUNT * 4
    yi_count = data[count_offset]
    ji_count = data[count_offset + 1]
    list_offset = count_offset + 4
    yi = tuple(strings[_unpack_u32(data, list_offset + index * 4)] for index in range(yi_count))
    ji_offset = list_offset + header.max_yi_count * 4
    ji = tuple(strings[_unpack_u32(data, ji_offset + index * 4)] for index in range(ji_count))
    return AlmanacDay(
        solar_date=header.start_date + timedelta(days=day_offset),
        lunar_date=fields[0],
        solar_term=fields[1],
        ganzhi=fields[2],
        wuxing=fields[3],
        chongsha=fields[4],
        zhishen=fields[5],
        jianchu=fields[6],
        taishen=fields[7],
        yi=yi,
        ji=ji,
    )
```

- [ ] **Step 4: Run the format tests**

Run:

```bash
python3 tools/test_generate_almanac_data.py
```

Expected: PASS with `Ran 2 tests`.

- [ ] **Step 5: Commit the format layer**

```bash
git add tools/generate_almanac_data.py tools/test_generate_almanac_data.py
git commit -m "test: 添加黄历数据包格式测试" \
  -m "- 定义 HDALM001 二进制 header 与字符串表布局" \
  -m "- 覆盖记录解码和 payload CRC 校验"
```

---

### Task 2: Golden Source Mapping And Full Data Generation

**Files:**
- Create: `tools/requirements-almanac.txt`
- Modify: `tools/generate_almanac_data.py`
- Modify: `tools/test_generate_almanac_data.py`
- Create: `data/almanac.bin`

- [ ] **Step 1: Pin the generation dependency**

Create `tools/requirements-almanac.txt`:

```text
lunar_python==1.4.8
```

- [ ] **Step 2: Install the generation dependency locally**

Run:

```bash
python3 -m pip install -r tools/requirements-almanac.txt
```

Expected: command exits 0 and installs `lunar_python==1.4.8`.

- [ ] **Step 3: Add golden mapping tests**

Append these tests inside `AlmanacPackageFormatTest` in `tools/test_generate_almanac_data.py`:

```python
    def test_build_day_maps_figma_date_from_lunar_python(self) -> None:
        day = gen.build_day(date(2026, 12, 21))

        self.assertEqual("冬月十三", day.lunar_date)
        self.assertEqual("", day.solar_term)
        self.assertEqual("丙午年 庚子月 己巳日 蛇日", day.ganzhi)
        self.assertEqual("五行大林木", day.wuxing)
        self.assertEqual("冲猪煞东", day.chongsha)
        self.assertEqual("值神玄武", day.zhishen)
        self.assertEqual("建除执日", day.jianchu)
        self.assertEqual("胎神占门床外正南", day.taishen)
        self.assertEqual(
            (
                "嫁娶",
                "冠笄",
                "祭祀",
                "祈福",
                "求嗣",
                "斋醮",
                "进人口",
                "会亲友",
                "伐木",
                "作梁",
                "开柱眼",
                "安床",
                "掘井",
                "捕捉",
                "畋猎",
            ),
            day.yi,
        )
        self.assertEqual(("开生坟", "破土", "行丧", "安葬"), day.ji)

    def test_build_day_covers_new_year_leap_month_solar_term_and_regular_day(self) -> None:
        lunar_new_year = gen.build_day(date(2026, 2, 17))
        leap_month = gen.build_day(date(2025, 7, 25))
        solar_term = gen.build_day(date(2026, 5, 21))
        regular = gen.build_day(date(2026, 3, 3))

        self.assertEqual("正月初一", lunar_new_year.lunar_date)
        self.assertEqual("丙午年 庚寅月 壬戌日 狗日", lunar_new_year.ganzhi)
        self.assertEqual("胎神仓库栖外东南", lunar_new_year.taishen)

        self.assertEqual("闰六月初一", leap_month.lunar_date)
        self.assertEqual("乙巳年 癸未月 乙未日 羊日", leap_month.ganzhi)

        self.assertEqual("四月初五", solar_term.lunar_date)
        self.assertEqual("小满", solar_term.solar_term)
        self.assertEqual("值神明堂", solar_term.zhishen)

        self.assertEqual("正月十五", regular.lunar_date)
        self.assertEqual("建除开日", regular.jianchu)
        self.assertEqual(("移徙", "入宅", "出火", "安门", "安葬"), regular.ji)
```

- [ ] **Step 4: Run the tests to verify mapping functions are missing**

Run:

```bash
python3 tools/test_generate_almanac_data.py
```

Expected: FAIL with `AttributeError: module 'generate_almanac_data' has no attribute 'build_day'`.

- [ ] **Step 5: Add lunar-python mapping and CLI**

Append this code to `tools/generate_almanac_data.py` after `unpack_record()`:

```python
def _require_lunar_python():
    try:
        from lunar_python import Solar  # type: ignore
    except ImportError as exc:
        raise SystemExit(
            "lunar_python==1.4.8 is required. Run: "
            "python3 -m pip install -r tools/requirements-almanac.txt"
        ) from exc
    return Solar


def _join_without_spaces(prefix: str, value: str) -> str:
    return prefix + value.replace(" ", "")


def _lunar_date_text(lunar) -> str:
    month = lunar.getMonthInChinese()
    if lunar.getMonth() < 0 and not month.startswith("闰"):
        month = "闰" + month
    if not month.endswith("月"):
        month += "月"
    return month + lunar.getDayInChinese()


def build_day(solar_date: date) -> AlmanacDay:
    Solar = _require_lunar_python()
    lunar = Solar.fromYmd(solar_date.year, solar_date.month, solar_date.day).getLunar()
    ganzhi = (
        f"{lunar.getYearInGanZhi()}年 "
        f"{lunar.getMonthInGanZhi()}月 "
        f"{lunar.getDayInGanZhi()}日 "
        f"{lunar.getDayShengXiao()}日"
    )
    return AlmanacDay(
        solar_date=solar_date,
        lunar_date=_lunar_date_text(lunar),
        solar_term=lunar.getJieQi(),
        ganzhi=ganzhi,
        wuxing="五行" + lunar.getDayNaYin(),
        chongsha=f"冲{lunar.getDayChongShengXiao()}煞{lunar.getDaySha()}",
        zhishen="值神" + lunar.getDayTianShen(),
        jianchu="建除" + lunar.getZhiXing() + "日",
        taishen=_join_without_spaces("胎神", lunar.getDayPositionTai()),
        yi=tuple(lunar.getDayYi()),
        ji=tuple(lunar.getDayJi()),
    )


def iter_dates(start: date, end: date) -> Iterable[date]:
    current = start
    while current <= end:
        yield current
        current += timedelta(days=1)


def build_days(start: date = START_DATE, end: date = END_DATE) -> list[AlmanacDay]:
    return [build_day(current) for current in iter_dates(start, end)]


def write_package(path: Path, days: Sequence[AlmanacDay]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(build_almanac_package(days))


def verify_golden() -> None:
    golden_dates = (
        date(2026, 12, 21),
        date(2026, 2, 17),
        date(2025, 7, 25),
        date(2026, 5, 21),
        date(2026, 3, 3),
    )
    for value in golden_dates:
        day = build_day(value)
        if not day.lunar_date or not day.ganzhi or not day.yi or not day.ji:
            raise SystemExit(f"golden date produced incomplete data: {value}")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate HomeDeck offline almanac data.")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--verify-golden", action="store_true")
    args = parser.parse_args(argv)

    if args.verify_golden:
        verify_golden()

    days = build_days()
    if len(days) != EXPECTED_DAY_COUNT:
        raise SystemExit(f"expected {EXPECTED_DAY_COUNT} days, generated {len(days)}")
    write_package(args.output, days)
    package = args.output.read_bytes()
    header = unpack_header(package)
    print(f"Generated {args.output.relative_to(ROOT)}")
    print(f"Date range: {header.start_date}..{header.end_date}")
    print(f"Days: {header.day_count}")
    print(f"Record size: {header.record_size}")
    print(f"Strings: {header.string_count}")
    print(f"Bytes: {len(package)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 6: Run Python tests**

Run:

```bash
python3 tools/test_generate_almanac_data.py
```

Expected: PASS with `Ran 4 tests`.

- [ ] **Step 7: Generate the full LittleFS data package**

Run:

```bash
python3 tools/generate_almanac_data.py --verify-golden --output data/almanac.bin
```

Expected output includes:

```text
Generated data/almanac.bin
Date range: 1900-01-01..2100-12-31
Days: 73414
```

- [ ] **Step 8: Verify the generated data file exists**

Run:

```bash
test -s data/almanac.bin && ls -lh data/almanac.bin
```

Expected: command exits 0 and prints a non-zero file size.

- [ ] **Step 9: Commit generator and generated data**

```bash
git add tools/requirements-almanac.txt tools/generate_almanac_data.py tools/test_generate_almanac_data.py data/almanac.bin
git commit -m "feat: 生成 1900-2100 离线黄历数据包" \
  -m "- 固定 lunar_python==1.4.8 作为生成期数据源" \
  -m "- 生成 LittleFS 使用的 almanac.bin 二进制数据" \
  -m "- 增加 Figma 日期、春节、闰月、节气日和普通日 golden 测试"
```

---

### Task 3: Native LittleFS Fake And Almanac Provider Tests

**Files:**
- Modify: `test/native/support/fake_arduino/LittleFS.h`
- Create: `test/native/support/almanac_fixture.h`
- Create: `test/native/test_almanac_provider/test_main.cpp`

- [ ] **Step 1: Expand the LittleFS fake with file reads**

Replace `test/native/support/fake_arduino/LittleFS.h` with:

```cpp
#pragma once

#include <algorithm>
#include <cstddef>
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

class FakeFile {
 public:
  FakeFile() = default;
  explicit FakeFile(const std::vector<std::uint8_t>* data) : data_(data) {
  }

  explicit operator bool() const {
    return data_ != nullptr;
  }

  std::size_t read(std::uint8_t* buffer, std::size_t length) {
    if (data_ == nullptr || buffer == nullptr) {
      return 0;
    }
    const std::size_t available = pos_ < data_->size() ? data_->size() - pos_ : 0;
    const std::size_t count = std::min(length, available);
    std::copy_n(data_->data() + pos_, count, buffer);
    pos_ += count;
    return count;
  }

  bool seek(std::uint32_t position) {
    if (data_ == nullptr || position > data_->size()) {
      return false;
    }
    pos_ = position;
    return true;
  }

  std::size_t size() const {
    return data_ != nullptr ? data_->size() : 0;
  }

  void close() {
    data_ = nullptr;
    pos_ = 0;
  }

 private:
  const std::vector<std::uint8_t>* data_ = nullptr;
  std::size_t pos_ = 0;
};

using File = FakeFile;

class LittleFSClass {
 public:
  bool begin() {
    began = true;
    return beginSucceeds;
  }

  void end() {
    ended = true;
  }

  File open(const char* path, const char*) {
    openPath = path != nullptr ? path : "";
    auto found = files.find(openPath);
    if (found == files.end()) {
      return File{};
    }
    return File(&found->second);
  }

  bool exists(const char* path) const {
    return files.find(path != nullptr ? path : "") != files.end();
  }

  bool beginSucceeds = true;
  bool began = false;
  bool ended = false;
  std::string openPath;
  std::map<std::string, std::vector<std::uint8_t>> files;
};

inline LittleFSClass LittleFS;

inline void fakeLittleFSReset() {
  LittleFS = LittleFSClass{};
}

inline void fakeLittleFSSetFile(const std::string& path, std::vector<std::uint8_t> data) {
  LittleFS.files[path] = std::move(data);
}
```

- [ ] **Step 2: Add a native fixture package builder**

Create `test/native/support/almanac_fixture.h`:

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace homedeck::test {

struct AlmanacFixtureDay {
  std::string lunarDate;
  std::string solarTerm;
  std::string ganzhi;
  std::string wuxing;
  std::string chongsha;
  std::string zhishen;
  std::string jianchu;
  std::string taishen;
  std::vector<std::string> yi;
  std::vector<std::string> ji;
};

inline void pushU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

inline void pushI16(std::vector<std::uint8_t>& out, std::int16_t value) {
  pushU16(out, static_cast<std::uint16_t>(value));
}

inline void pushU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

inline std::uint32_t crc32(const std::vector<std::uint8_t>& data, std::size_t start) {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (std::size_t index = start; index < data.size(); ++index) {
    crc ^= data[index];
    for (int bit = 0; bit < 8; ++bit) {
      const std::uint32_t mask = (crc & 1U) ? 0xEDB88320U : 0U;
      crc = (crc >> 1U) ^ mask;
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

inline std::uint32_t indexOf(std::vector<std::string>& strings, const std::string& value) {
  for (std::uint32_t index = 0; index < strings.size(); ++index) {
    if (strings[index] == value) {
      return index;
    }
  }
  strings.push_back(value);
  return static_cast<std::uint32_t>(strings.size() - 1);
}

inline std::vector<std::uint8_t> buildAlmanacFixturePackage(
    int startYear,
    int startMonth,
    int startDay,
    const std::vector<AlmanacFixtureDay>& days) {
  std::vector<std::string> strings{""};
  std::uint8_t maxYi = 0;
  std::uint8_t maxJi = 0;
  for (const auto& day : days) {
    indexOf(strings, day.lunarDate);
    indexOf(strings, day.solarTerm);
    indexOf(strings, day.ganzhi);
    indexOf(strings, day.wuxing);
    indexOf(strings, day.chongsha);
    indexOf(strings, day.zhishen);
    indexOf(strings, day.jianchu);
    indexOf(strings, day.taishen);
    for (const auto& value : day.yi) {
      indexOf(strings, value);
    }
    for (const auto& value : day.ji) {
      indexOf(strings, value);
    }
    maxYi = static_cast<std::uint8_t>(std::max<int>(maxYi, day.yi.size()));
    maxJi = static_cast<std::uint8_t>(std::max<int>(maxJi, day.ji.size()));
  }

  const std::uint16_t recordSize = static_cast<std::uint16_t>(36 + (maxYi + maxJi) * 4);
  std::vector<std::uint8_t> records;
  for (const auto& day : days) {
    pushU32(records, indexOf(strings, day.lunarDate));
    pushU32(records, indexOf(strings, day.solarTerm));
    pushU32(records, indexOf(strings, day.ganzhi));
    pushU32(records, indexOf(strings, day.wuxing));
    pushU32(records, indexOf(strings, day.chongsha));
    pushU32(records, indexOf(strings, day.zhishen));
    pushU32(records, indexOf(strings, day.jianchu));
    pushU32(records, indexOf(strings, day.taishen));
    records.push_back(static_cast<std::uint8_t>(day.yi.size()));
    records.push_back(static_cast<std::uint8_t>(day.ji.size()));
    pushU16(records, 0);
    for (const auto& value : day.yi) {
      pushU32(records, indexOf(strings, value));
    }
    for (int index = static_cast<int>(day.yi.size()); index < maxYi; ++index) {
      pushU32(records, 0);
    }
    for (const auto& value : day.ji) {
      pushU32(records, indexOf(strings, value));
    }
    for (int index = static_cast<int>(day.ji.size()); index < maxJi; ++index) {
      pushU32(records, 0);
    }
  }

  std::vector<std::uint8_t> stringTable;
  std::vector<std::uint32_t> offsets;
  std::vector<std::uint8_t> blob;
  for (const auto& value : strings) {
    offsets.push_back(static_cast<std::uint32_t>(blob.size()));
    blob.insert(blob.end(), value.begin(), value.end());
  }
  offsets.push_back(static_cast<std::uint32_t>(blob.size()));
  for (std::uint32_t offset : offsets) {
    pushU32(stringTable, offset);
  }
  stringTable.insert(stringTable.end(), blob.begin(), blob.end());

  std::vector<std::uint8_t> package;
  package.insert(package.end(), {'H', 'D', 'A', 'L', 'M', '0', '0', '1'});
  pushU16(package, 1);
  pushU16(package, 64);
  pushI16(package, static_cast<std::int16_t>(startYear));
  package.push_back(static_cast<std::uint8_t>(startMonth));
  package.push_back(static_cast<std::uint8_t>(startDay));
  pushI16(package, static_cast<std::int16_t>(startYear));
  package.push_back(static_cast<std::uint8_t>(startMonth));
  package.push_back(static_cast<std::uint8_t>(startDay + static_cast<int>(days.size()) - 1));
  pushU32(package, static_cast<std::uint32_t>(days.size()));
  pushU16(package, recordSize);
  package.push_back(maxYi);
  package.push_back(maxJi);
  pushU32(package, 64);
  pushU32(package, static_cast<std::uint32_t>(64 + records.size()));
  pushU32(package, static_cast<std::uint32_t>(strings.size()));
  pushU32(package, static_cast<std::uint32_t>(stringTable.size()));
  pushU32(package, 0);
  package.resize(64, 0);
  package.insert(package.end(), records.begin(), records.end());
  package.insert(package.end(), stringTable.begin(), stringTable.end());

  const std::uint32_t crc = crc32(package, 64);
  package[44] = static_cast<std::uint8_t>(crc & 0xFF);
  package[45] = static_cast<std::uint8_t>((crc >> 8) & 0xFF);
  package[46] = static_cast<std::uint8_t>((crc >> 16) & 0xFF);
  package[47] = static_cast<std::uint8_t>((crc >> 24) & 0xFF);
  return package;
}

inline std::vector<std::uint8_t> buildSingleDayFixturePackage() {
  return buildAlmanacFixturePackage(
      1900,
      1,
      1,
      {{"腊月初一",
        "",
        "己亥年 丙子月 甲子日 鼠日",
        "五行海中金",
        "冲马煞南",
        "值神青龙",
        "建除建日",
        "胎神占门碓外东南",
        {"祭祀", "祈福"},
        {"嫁娶"}}});
}

}  // namespace homedeck::test
```

- [ ] **Step 3: Write failing provider tests**

Create `test/native/test_almanac_provider/test_main.cpp`:

```cpp
#include <unity.h>

#include <LittleFS.h>

#include <vector>

#include "almanac_fixture.h"
#include "almanac_provider.h"

void setUp() {
  fakeLittleFSReset();
}

void tearDown() {
}

void test_lookup_returns_false_when_file_is_missing() {
  homedeck::AlmanacProvider provider;
  homedeck::AlmanacDayData day{};

  TEST_ASSERT_FALSE(provider.lookup(1900, 1, 1, &day));
  TEST_ASSERT_TRUE(LittleFS.began);
  TEST_ASSERT_TRUE(LittleFS.ended);
}

void test_lookup_reads_first_day_from_fixture_package() {
  fakeLittleFSSetFile("/almanac.bin", homedeck::test::buildSingleDayFixturePackage());
  homedeck::AlmanacProvider provider;
  homedeck::AlmanacDayData day{};

  TEST_ASSERT_TRUE(provider.lookup(1900, 1, 1, &day));

  TEST_ASSERT_EQUAL_STRING("腊月初一", day.lunarDate.c_str());
  TEST_ASSERT_EQUAL_STRING("", day.solarTerm.c_str());
  TEST_ASSERT_EQUAL_STRING("己亥年 丙子月 甲子日 鼠日", day.ganzhi.c_str());
  TEST_ASSERT_EQUAL_STRING("五行海中金", day.wuxing.c_str());
  TEST_ASSERT_EQUAL_STRING("冲马煞南", day.chongsha.c_str());
  TEST_ASSERT_EQUAL_STRING("值神青龙", day.zhishen.c_str());
  TEST_ASSERT_EQUAL_STRING("建除建日", day.jianchu.c_str());
  TEST_ASSERT_EQUAL_STRING("胎神占门碓外东南", day.taishen.c_str());
  TEST_ASSERT_EQUAL_STRING("祭祀 祈福", day.yi.c_str());
  TEST_ASSERT_EQUAL_STRING("嫁娶", day.ji.c_str());
}

void test_lookup_rejects_dates_outside_package_range() {
  fakeLittleFSSetFile("/almanac.bin", homedeck::test::buildSingleDayFixturePackage());
  homedeck::AlmanacProvider provider;
  homedeck::AlmanacDayData day{};

  TEST_ASSERT_FALSE(provider.lookup(1899, 12, 31, &day));
  TEST_ASSERT_FALSE(provider.lookup(1900, 1, 2, &day));
}

void test_lookup_rejects_bad_magic() {
  std::vector<std::uint8_t> package = homedeck::test::buildSingleDayFixturePackage();
  package[0] = 'X';
  fakeLittleFSSetFile("/almanac.bin", package);
  homedeck::AlmanacProvider provider;
  homedeck::AlmanacDayData day{};

  TEST_ASSERT_FALSE(provider.lookup(1900, 1, 1, &day));
}

void test_lookup_rejects_bad_crc() {
  std::vector<std::uint8_t> package = homedeck::test::buildSingleDayFixturePackage();
  package.back() ^= 0x01;
  fakeLittleFSSetFile("/almanac.bin", package);
  homedeck::AlmanacProvider provider;
  homedeck::AlmanacDayData day{};

  TEST_ASSERT_FALSE(provider.lookup(1900, 1, 1, &day));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_lookup_returns_false_when_file_is_missing);
  RUN_TEST(test_lookup_reads_first_day_from_fixture_package);
  RUN_TEST(test_lookup_rejects_dates_outside_package_range);
  RUN_TEST(test_lookup_rejects_bad_magic);
  RUN_TEST(test_lookup_rejects_bad_crc);
  return UNITY_END();
}
```

- [ ] **Step 4: Run provider tests to verify they fail**

Run:

```bash
pio test -e native --filter native/test_almanac_provider
```

Expected: FAIL to compile because `almanac_provider.h` does not exist.

- [ ] **Step 5: Keep the failing provider tests for the next task**

```bash
git status --short
```

Expected: shows the LittleFS fake, fixture helper, and provider test files as intentional uncommitted changes. Do not commit this failing state; Task 4 commits the tests together with the passing provider implementation.

---

### Task 4: Device-Side Almanac Provider

**Files:**
- Create: `src/almanac_provider.h`
- Create: `src/almanac_provider.cpp`
- Test: `test/native/test_almanac_provider/test_main.cpp`

- [ ] **Step 1: Add the provider header**

Create `src/almanac_provider.h`:

```cpp
#pragma once

#include <string>

namespace homedeck {

struct AlmanacDayData {
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
};

class AlmanacProvider {
 public:
  bool lookup(int year, int month, int day, AlmanacDayData* out) const;
};

}  // namespace homedeck
```

- [ ] **Step 2: Add the provider implementation**

Create `src/almanac_provider.cpp`:

```cpp
#include "almanac_provider.h"

#include <LittleFS.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace homedeck {
namespace {

constexpr const char* kAlmanacPath = "/almanac.bin";
constexpr std::array<std::uint8_t, 8> kMagic = {'H', 'D', 'A', 'L', 'M', '0', '0', '1'};
constexpr std::uint16_t kFormatVersion = 1;
constexpr std::uint16_t kHeaderSize = 64;
constexpr std::uint32_t kTextFieldCount = 8;

struct AlmanacHeader {
  std::uint16_t formatVersion = 0;
  std::uint16_t headerSize = 0;
  int startYear = 0;
  int startMonth = 0;
  int startDay = 0;
  int endYear = 0;
  int endMonth = 0;
  int endDay = 0;
  std::uint32_t dayCount = 0;
  std::uint16_t recordSize = 0;
  std::uint8_t maxYiCount = 0;
  std::uint8_t maxJiCount = 0;
  std::uint32_t recordsOffset = 0;
  std::uint32_t stringTableOffset = 0;
  std::uint32_t stringCount = 0;
  std::uint32_t stringTableSize = 0;
  std::uint32_t payloadCrc32 = 0;
};

std::uint16_t readU16(const std::array<std::uint8_t, kHeaderSize>& bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(bytes[offset]) |
      static_cast<std::uint16_t>(bytes[offset + 1] << 8U);
}

std::int16_t readI16(const std::array<std::uint8_t, kHeaderSize>& bytes, std::size_t offset) {
  return static_cast<std::int16_t>(readU16(bytes, offset));
}

std::uint32_t readU32(const std::array<std::uint8_t, kHeaderSize>& bytes, std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) |
      (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
      (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
      (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

std::uint16_t readU16Buffer(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(bytes[offset]) |
      static_cast<std::uint16_t>(bytes[offset + 1] << 8U);
}

std::uint32_t readU32Buffer(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) |
      (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
      (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
      (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

std::uint32_t crc32Update(std::uint32_t crc, const std::uint8_t* data, std::size_t length) {
  for (std::size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (int bit = 0; bit < 8; ++bit) {
      const std::uint32_t mask = (crc & 1U) ? 0xEDB88320U : 0U;
      crc = (crc >> 1U) ^ mask;
    }
  }
  return crc;
}

int daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int>(doe) - 719468;
}

bool readExact(File& file, std::uint32_t offset, std::uint8_t* buffer, std::size_t length) {
  if (!file.seek(offset)) {
    return false;
  }
  return file.read(buffer, length) == length;
}

bool readHeader(File& file, AlmanacHeader* out) {
  if (out == nullptr) {
    return false;
  }
  std::array<std::uint8_t, kHeaderSize> bytes{};
  if (!readExact(file, 0, bytes.data(), bytes.size())) {
    return false;
  }
  for (std::size_t index = 0; index < kMagic.size(); ++index) {
    if (bytes[index] != kMagic[index]) {
      return false;
    }
  }

  AlmanacHeader header{};
  header.formatVersion = readU16(bytes, 8);
  header.headerSize = readU16(bytes, 10);
  header.startYear = readI16(bytes, 12);
  header.startMonth = bytes[14];
  header.startDay = bytes[15];
  header.endYear = readI16(bytes, 16);
  header.endMonth = bytes[18];
  header.endDay = bytes[19];
  header.dayCount = readU32(bytes, 20);
  header.recordSize = readU16(bytes, 24);
  header.maxYiCount = bytes[26];
  header.maxJiCount = bytes[27];
  header.recordsOffset = readU32(bytes, 28);
  header.stringTableOffset = readU32(bytes, 32);
  header.stringCount = readU32(bytes, 36);
  header.stringTableSize = readU32(bytes, 40);
  header.payloadCrc32 = readU32(bytes, 44);

  if (header.formatVersion != kFormatVersion || header.headerSize != kHeaderSize) {
    return false;
  }
  if (header.recordsOffset != kHeaderSize || header.recordSize < 36 || header.stringCount == 0) {
    return false;
  }
  if (header.stringTableOffset <= header.recordsOffset) {
    return false;
  }
  if (header.stringTableOffset + header.stringTableSize > file.size()) {
    return false;
  }
  *out = header;
  return true;
}

bool verifyPayloadCrc(File& file, const AlmanacHeader& header) {
  if (!file.seek(header.recordsOffset)) {
    return false;
  }
  std::array<std::uint8_t, 256> buffer{};
  std::uint32_t remaining = header.stringTableOffset + header.stringTableSize - header.recordsOffset;
  std::uint32_t crc = 0xFFFFFFFFU;
  while (remaining > 0) {
    const std::size_t chunk = remaining < buffer.size() ? remaining : buffer.size();
    if (file.read(buffer.data(), chunk) != chunk) {
      return false;
    }
    crc = crc32Update(crc, buffer.data(), chunk);
    remaining -= chunk;
  }
  return (crc ^ 0xFFFFFFFFU) == header.payloadCrc32;
}

bool dateOffset(const AlmanacHeader& header, int year, int month, int day, std::uint32_t* out) {
  if (out == nullptr || month < 1 || month > 12 || day < 1 || day > 31) {
    return false;
  }
  const int start = daysFromCivil(header.startYear, header.startMonth, header.startDay);
  const int end = daysFromCivil(header.endYear, header.endMonth, header.endDay);
  const int current = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
  if (current < start || current > end) {
    return false;
  }
  const int offset = current - start;
  if (offset < 0 || static_cast<std::uint32_t>(offset) >= header.dayCount) {
    return false;
  }
  *out = static_cast<std::uint32_t>(offset);
  return true;
}

bool readString(File& file, const AlmanacHeader& header, std::uint32_t index, std::string* out) {
  if (out == nullptr || index >= header.stringCount) {
    return false;
  }
  std::array<std::uint8_t, 8> offsetBytes{};
  const std::uint32_t offsetPos = header.stringTableOffset + index * 4;
  if (!readExact(file, offsetPos, offsetBytes.data(), offsetBytes.size())) {
    return false;
  }
  const std::uint32_t start = static_cast<std::uint32_t>(offsetBytes[0]) |
      (static_cast<std::uint32_t>(offsetBytes[1]) << 8U) |
      (static_cast<std::uint32_t>(offsetBytes[2]) << 16U) |
      (static_cast<std::uint32_t>(offsetBytes[3]) << 24U);
  const std::uint32_t end = static_cast<std::uint32_t>(offsetBytes[4]) |
      (static_cast<std::uint32_t>(offsetBytes[5]) << 8U) |
      (static_cast<std::uint32_t>(offsetBytes[6]) << 16U) |
      (static_cast<std::uint32_t>(offsetBytes[7]) << 24U);
  if (end < start) {
    return false;
  }
  const std::uint32_t blobOffset = header.stringTableOffset + (header.stringCount + 1) * 4;
  const std::uint32_t length = end - start;
  std::vector<std::uint8_t> bytes(length);
  if (length > 0 && !readExact(file, blobOffset + start, bytes.data(), bytes.size())) {
    return false;
  }
  out->assign(bytes.begin(), bytes.end());
  return true;
}

bool appendIndexedText(
    File& file,
    const AlmanacHeader& header,
    const std::vector<std::uint8_t>& record,
    std::size_t indexOffset,
    std::string* out) {
  const std::uint32_t index = readU32Buffer(record, indexOffset);
  return readString(file, header, index, out);
}

bool appendTerms(
    File& file,
    const AlmanacHeader& header,
    const std::vector<std::uint8_t>& record,
    std::size_t offset,
    std::uint8_t count,
    std::string* out) {
  if (out == nullptr) {
    return false;
  }
  out->clear();
  for (std::uint8_t index = 0; index < count; ++index) {
    std::string term;
    if (!readString(file, header, readU32Buffer(record, offset + index * 4), &term)) {
      return false;
    }
    if (!out->empty()) {
      *out += " ";
    }
    *out += term;
  }
  return true;
}

bool decodeRecord(File& file, const AlmanacHeader& header, std::uint32_t dayOffset, AlmanacDayData* out) {
  if (out == nullptr) {
    return false;
  }
  std::vector<std::uint8_t> record(header.recordSize);
  const std::uint32_t recordOffset = header.recordsOffset + dayOffset * header.recordSize;
  if (!readExact(file, recordOffset, record.data(), record.size())) {
    return false;
  }
  if (!appendIndexedText(file, header, record, 0, &out->lunarDate) ||
      !appendIndexedText(file, header, record, 4, &out->solarTerm) ||
      !appendIndexedText(file, header, record, 8, &out->ganzhi) ||
      !appendIndexedText(file, header, record, 12, &out->wuxing) ||
      !appendIndexedText(file, header, record, 16, &out->chongsha) ||
      !appendIndexedText(file, header, record, 20, &out->zhishen) ||
      !appendIndexedText(file, header, record, 24, &out->jianchu) ||
      !appendIndexedText(file, header, record, 28, &out->taishen)) {
    return false;
  }
  const std::uint8_t yiCount = record[32];
  const std::uint8_t jiCount = record[33];
  if (yiCount > header.maxYiCount || jiCount > header.maxJiCount) {
    return false;
  }
  const std::size_t yiOffset = 36;
  const std::size_t jiOffset = yiOffset + header.maxYiCount * 4;
  return appendTerms(file, header, record, yiOffset, yiCount, &out->yi) &&
      appendTerms(file, header, record, jiOffset, jiCount, &out->ji);
}

}  // namespace

bool AlmanacProvider::lookup(int year, int month, int day, AlmanacDayData* out) const {
  if (out == nullptr || !LittleFS.begin()) {
    return false;
  }

  bool ok = false;
  File file = LittleFS.open(kAlmanacPath, "r");
  if (file) {
    AlmanacHeader header{};
    std::uint32_t offset = 0;
    ok = readHeader(file, &header) &&
        verifyPayloadCrc(file, header) &&
        dateOffset(header, year, month, day, &offset) &&
        decodeRecord(file, header, offset, out);
    file.close();
  }
  LittleFS.end();
  return ok;
}

}  // namespace homedeck
```

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
  -m "- 增加 almanac.bin fixture 生成器和 provider native 测试" \
  -m "- 从 LittleFS almanac.bin 按日期读取单日记录" \
  -m "- 校验 header、日期范围、字符串索引和 payload CRC" \
  -m "- 解码 Figma 首页所需黄历字段"
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
