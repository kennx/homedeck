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


def _build_string_table(days: Sequence[AlmanacDay]) -> tuple[dict[str, int], bytes, int]:
    strings: list[str] = [""]
    indexes: dict[str, int] = {"": 0}
    for day in days:
        for value in (*_text_fields(day), *day.yi, *day.ji):
            if value not in indexes:
                indexes[value] = len(strings)
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
    return indexes, bytes(table), len(strings)


def _build_records(
    days: Sequence[AlmanacDay], string_indexes: dict[str, int]
) -> tuple[bytes, int, int, int]:
    max_yi_count = max((len(day.yi) for day in days), default=0)
    max_ji_count = max((len(day.ji) for day in days), default=0)
    if max_yi_count > 255 or max_ji_count > 255:
        raise ValueError(
            f"yi/ji count exceeds uint8 limit: yi={max_yi_count}, ji={max_ji_count}"
        )

    record_size = 36 + (max_yi_count + max_ji_count) * 4
    records = bytearray()
    for day in days:
        for value in _text_fields(day):
            records.extend(_pack_u32(string_indexes[value]))
        records.extend(bytes((len(day.yi), len(day.ji))))
        records.extend(_pack_u16(0))
        for value in day.yi:
            records.extend(_pack_u32(string_indexes[value]))
        for _ in range(max_yi_count - len(day.yi)):
            records.extend(_pack_u32(0))
        for value in day.ji:
            records.extend(_pack_u32(string_indexes[value]))
        for _ in range(max_ji_count - len(day.ji)):
            records.extend(_pack_u32(0))

    return bytes(records), record_size, max_yi_count, max_ji_count


def _build_header(
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
            raise ValueError(
                f"non-contiguous date at index {index}: {days[index].solar_date}"
            )

    string_indexes, string_table, string_count = _build_string_table(days)
    records, record_size, max_yi_count, max_ji_count = _build_records(
        days, string_indexes
    )
    records_offset = HEADER_SIZE
    string_table_offset = records_offset + len(records)
    payload = records + string_table
    payload_crc32 = zlib.crc32(payload) & 0xFFFFFFFF
    header = _build_header(
        start_date=days[0].solar_date,
        end_date=days[-1].solar_date,
        day_count=len(days),
        record_size=record_size,
        max_yi_count=max_yi_count,
        max_ji_count=max_ji_count,
        records_offset=records_offset,
        string_table_offset=string_table_offset,
        string_count=string_count,
        string_table_size=len(string_table),
        payload_crc32=payload_crc32,
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
    payload_end = header.string_table_offset + header.string_table_size
    payload = data[header.records_offset : payload_end]
    return (zlib.crc32(payload) & 0xFFFFFFFF) == header.payload_crc32


def unpack_string_table(data: bytes | bytearray, header: AlmanacHeader) -> list[str]:
    base = header.string_table_offset
    offsets = [
        _unpack_u32(data, base + index * 4) for index in range(header.string_count + 1)
    ]
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
    fields = [
        strings[_unpack_u32(data, offset + index * 4)]
        for index in range(TEXT_FIELD_COUNT)
    ]
    count_offset = offset + TEXT_FIELD_COUNT * 4
    yi_count = data[count_offset]
    ji_count = data[count_offset + 1]
    list_offset = count_offset + 4
    yi = tuple(
        strings[_unpack_u32(data, list_offset + index * 4)]
        for index in range(yi_count)
    )
    ji_offset = list_offset + header.max_yi_count * 4
    ji = tuple(
        strings[_unpack_u32(data, ji_offset + index * 4)]
        for index in range(ji_count)
    )
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


def _output_path(path: Path) -> Path:
    return path if path.is_absolute() else ROOT / path


def _display_path(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


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

    output_path = _output_path(args.output)
    write_package(output_path, days)
    package = output_path.read_bytes()
    header = unpack_header(package)
    print(f"Generated {_display_path(output_path)}")
    print(f"Date range: {header.start_date}..{header.end_date}")
    print(f"Days: {header.day_count}")
    print(f"Record size: {header.record_size}")
    print(f"Strings: {header.string_count}")
    print(f"Bytes: {len(package)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
