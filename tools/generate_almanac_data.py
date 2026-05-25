#!/usr/bin/env python3
from __future__ import annotations

import argparse
import struct
import zlib
from dataclasses import dataclass
from datetime import date, timedelta
from importlib.metadata import PackageNotFoundError, version
from pathlib import Path
from typing import Iterable, Sequence


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "data" / "almanac.bin"
MAGIC = b"HDALM001"
FORMAT_VERSION = 2
HEADER_SIZE = 64
START_DATE = date(1900, 1, 1)
END_DATE = date(2100, 12, 31)
EXPECTED_DAY_COUNT = 73414
TEXT_FIELD_COUNT = 11
RECORD_OFFSET_SIZE = 3
REQUIRED_LUNAR_PYTHON_VERSION = "1.4.8"
MAX_ALMANAC_BYTES = 0x360000 - 65536
INSTALL_HINT = (
    "python3 -m venv .venv-almanac && "
    ". .venv-almanac/bin/activate && "
    "python3 -m pip install -r tools/requirements-almanac.txt"
)


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
    max_yi_count: int
    max_ji_count: int
    term_count: int
    record_offsets_offset: int
    records_offset: int
    string_table_offset: int
    string_count: int
    string_table_size: int
    payload_crc32: int
    record_offset_size: int


GOLDEN_DAYS = {
    date(2026, 12, 21): AlmanacDay(
        solar_date=date(2026, 12, 21),
        lunar_date="冬月十三",
        solar_term="",
        ganzhi="丙午年 庚子月 己巳日 蛇日",
        wuxing="五行大林木",
        chongsha="冲猪煞东",
        zhishen="值神玄武",
        jianchu="建除执日",
        taishen="胎神占门床外正南",
        yi=(
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
        ji=("开生坟", "破土", "行丧", "安葬"),
    ),
    date(2026, 2, 17): AlmanacDay(
        solar_date=date(2026, 2, 17),
        lunar_date="正月初一",
        solar_term="",
        ganzhi="丙午年 庚寅月 壬戌日 狗日",
        wuxing="五行大海水",
        chongsha="冲龙煞北",
        zhishen="值神司命",
        jianchu="建除成日",
        taishen="胎神仓库栖外东南",
        yi=("祭祀", "塞穴", "结网", "破土", "谢土", "安葬", "移柩", "除服", "成服", "馀事勿取"),
        ji=("嫁娶", "入宅"),
    ),
    date(2025, 7, 25): AlmanacDay(
        solar_date=date(2025, 7, 25),
        lunar_date="闰六月初一",
        solar_term="",
        ganzhi="乙巳年 癸未月 乙未日 羊日",
        wuxing="五行沙中金",
        chongsha="冲牛煞西",
        zhishen="值神玄武",
        jianchu="建除建日",
        taishen="胎神碓磨厕房内北",
        yi=("嫁娶", "祭祀", "出行", "裁衣", "冠笄", "交易", "雕刻", "纳财", "造畜稠", "造车器", "教牛马"),
        ji=("移徙", "入宅", "栽种", "动土", "破土", "作灶", "安葬", "行丧", "伐木", "上梁"),
    ),
    date(2026, 5, 21): AlmanacDay(
        solar_date=date(2026, 5, 21),
        lunar_date="四月初五",
        solar_term="小满",
        ganzhi="丙午年 癸巳月 乙未日 羊日",
        wuxing="五行沙中金",
        chongsha="冲牛煞西",
        zhishen="值神明堂",
        jianchu="建除满日",
        taishen="胎神碓磨厕房内北",
        yi=("开光", "纳采", "裁衣", "冠笄", "安床", "作灶", "进人口", "造仓", "塞穴"),
        ji=("嫁娶", "栽种", "修造", "动土", "出行", "伐木", "作梁", "安葬", "谢土"),
    ),
    date(2026, 3, 3): AlmanacDay(
        solar_date=date(2026, 3, 3),
        lunar_date="正月十五",
        solar_term="",
        ganzhi="丙午年 庚寅月 丙子日 鼠日",
        wuxing="五行涧下水",
        chongsha="冲马煞南",
        zhishen="值神青龙",
        jianchu="建除开日",
        taishen="胎神厨灶碓外西南",
        yi=("纳采", "嫁娶", "祭祀", "祈福", "出行", "开市", "会亲友", "动土", "破土", "启钻"),
        ji=("移徙", "入宅", "出火", "安门", "安葬"),
    ),
}


def _pack_u16(value: int) -> bytes:
    return struct.pack("<H", value)


def _pack_i16(value: int) -> bytes:
    return struct.pack("<h", value)


def _pack_u32(value: int) -> bytes:
    return struct.pack("<I", value)


def _pack_u24(value: int) -> bytes:
    if value < 0 or value > 0xFFFFFF:
        raise ValueError(f"value does not fit in uint24: {value}")
    return bytes((value & 0xFF, (value >> 8) & 0xFF, (value >> 16) & 0xFF))


def _unpack_u16(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def _unpack_i16(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<h", data, offset)[0]


def _unpack_u32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def _unpack_u24(data: bytes | bytearray, offset: int) -> int:
    return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16)


def _date_bytes(value: date) -> bytes:
    return _pack_i16(value.year) + bytes((value.month, value.day))


def _read_date(data: bytes | bytearray, offset: int) -> date:
    return date(_unpack_i16(data, offset), data[offset + 2], data[offset + 3])


def _strip_suffix(value: str, suffix: str) -> str:
    if not value.endswith(suffix):
        raise ValueError(f"expected '{value}' to end with '{suffix}'")
    return value[: -len(suffix)]


def _split_ganzhi(value: str) -> tuple[str, str, str, str]:
    parts = value.split()
    if len(parts) != 4:
        raise ValueError(f"unexpected ganzhi format: {value}")
    return (
        _strip_suffix(parts[0], "年"),
        _strip_suffix(parts[1], "月"),
        _strip_suffix(parts[2], "日"),
        _strip_suffix(parts[3], "日"),
    )


def _text_fields(day: AlmanacDay) -> tuple[str, ...]:
    year_ganzhi, month_ganzhi, day_ganzhi, day_shengxiao = _split_ganzhi(day.ganzhi)
    return (
        day.lunar_date,
        day.solar_term,
        year_ganzhi,
        month_ganzhi,
        day_ganzhi,
        day_shengxiao,
        day.wuxing,
        day.chongsha,
        day.zhishen,
        day.jianchu,
        day.taishen,
    )


def _build_string_table(
    days: Sequence[AlmanacDay],
) -> tuple[dict[str, int], dict[str, int], bytes, int, int]:
    terms: list[str] = []
    term_ids: dict[str, int] = {}
    for day in days:
        for value in (*day.yi, *day.ji):
            if value not in term_ids:
                term_ids[value] = len(terms)
                terms.append(value)
    if len(terms) > 255:
        raise ValueError(f"term count exceeds uint8 limit: {len(terms)}")

    strings: list[str] = ["", *terms]
    indexes: dict[str, int] = {value: index for index, value in enumerate(strings)}
    for day in days:
        for value in _text_fields(day):
            if value not in indexes:
                indexes[value] = len(strings)
                strings.append(value)
    if len(strings) > 65535:
        raise ValueError(f"string count exceeds uint16 limit: {len(strings)}")

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
    return indexes, term_ids, bytes(table), len(strings), len(terms)


def _build_records(
    days: Sequence[AlmanacDay], string_indexes: dict[str, int], term_ids: dict[str, int]
) -> tuple[bytes, list[int], int, int]:
    max_yi_count = max((len(day.yi) for day in days), default=0)
    max_ji_count = max((len(day.ji) for day in days), default=0)
    if max_yi_count > 255 or max_ji_count > 255:
        raise ValueError(
            f"yi/ji count exceeds uint8 limit: yi={max_yi_count}, ji={max_ji_count}"
        )

    records = bytearray()
    record_offsets = [0]
    for day in days:
        for value in _text_fields(day):
            records.extend(_pack_u16(string_indexes[value]))
        records.extend(bytes((len(day.yi), len(day.ji))))
        for value in day.yi:
            records.append(term_ids[value])
        for value in day.ji:
            records.append(term_ids[value])
        record_offsets.append(len(records))

    return bytes(records), record_offsets, max_yi_count, max_ji_count


def _build_header(
    *,
    start_date: date,
    end_date: date,
    day_count: int,
    max_yi_count: int,
    max_ji_count: int,
    term_count: int,
    record_offsets_offset: int,
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
    header.extend(bytes((max_yi_count, max_ji_count)))
    header.extend(_pack_u16(term_count))
    header.extend(_pack_u32(record_offsets_offset))
    header.extend(_pack_u32(records_offset))
    header.extend(_pack_u32(string_table_offset))
    header.extend(_pack_u32(string_count))
    header.extend(_pack_u32(string_table_size))
    header.extend(_pack_u32(payload_crc32))
    header.extend(bytes((RECORD_OFFSET_SIZE,)))
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

    string_indexes, term_ids, string_table, string_count, term_count = _build_string_table(days)
    records, record_offsets, max_yi_count, max_ji_count = _build_records(
        days, string_indexes, term_ids
    )
    record_offsets_blob = b"".join(_pack_u24(offset) for offset in record_offsets)
    record_offsets_offset = HEADER_SIZE
    records_offset = record_offsets_offset + len(record_offsets_blob)
    string_table_offset = records_offset + len(records)
    payload = record_offsets_blob + records + string_table
    payload_crc32 = zlib.crc32(payload) & 0xFFFFFFFF
    header = _build_header(
        start_date=days[0].solar_date,
        end_date=days[-1].solar_date,
        day_count=len(days),
        max_yi_count=max_yi_count,
        max_ji_count=max_ji_count,
        term_count=term_count,
        record_offsets_offset=record_offsets_offset,
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
        max_yi_count=data[24],
        max_ji_count=data[25],
        term_count=_unpack_u16(data, 26),
        record_offsets_offset=_unpack_u32(data, 28),
        records_offset=_unpack_u32(data, 32),
        string_table_offset=_unpack_u32(data, 36),
        string_count=_unpack_u32(data, 40),
        string_table_size=_unpack_u32(data, 44),
        payload_crc32=_unpack_u32(data, 48),
        record_offset_size=data[52],
    )


def verify_payload_crc(data: bytes | bytearray, header: AlmanacHeader) -> bool:
    payload_end = header.string_table_offset + header.string_table_size
    payload = data[header.header_size : payload_end]
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
    if header.record_offset_size != RECORD_OFFSET_SIZE:
        raise ValueError(f"unsupported record offset size: {header.record_offset_size}")
    record_relative_offset = _unpack_u24(
        data, header.record_offsets_offset + day_offset * header.record_offset_size
    )
    offset = header.records_offset + record_relative_offset
    fields = [
        strings[_unpack_u16(data, offset + index * 2)]
        for index in range(TEXT_FIELD_COUNT)
    ]
    count_offset = offset + TEXT_FIELD_COUNT * 2
    yi_count = data[count_offset]
    ji_count = data[count_offset + 1]
    list_offset = count_offset + 2
    yi = tuple(
        strings[1 + data[list_offset + index]]
        for index in range(yi_count)
    )
    ji_offset = list_offset + yi_count
    ji = tuple(
        strings[1 + data[ji_offset + index]]
        for index in range(ji_count)
    )
    return AlmanacDay(
        solar_date=header.start_date + timedelta(days=day_offset),
        lunar_date=fields[0],
        solar_term=fields[1],
        ganzhi=f"{fields[2]}年 {fields[3]}月 {fields[4]}日 {fields[5]}日",
        wuxing=fields[6],
        chongsha=fields[7],
        zhishen=fields[8],
        jianchu=fields[9],
        taishen=fields[10],
        yi=yi,
        ji=ji,
    )


def _require_lunar_python():
    try:
        installed_version = version("lunar_python")
    except PackageNotFoundError as exc:
        raise SystemExit(
            f"lunar_python=={REQUIRED_LUNAR_PYTHON_VERSION} is required. Run: "
            f"{INSTALL_HINT}"
        ) from exc
    if installed_version != REQUIRED_LUNAR_PYTHON_VERSION:
        raise SystemExit(
            f"lunar_python=={REQUIRED_LUNAR_PYTHON_VERSION} is required; "
            f"found {installed_version}. Run: {INSTALL_HINT}"
        )
    try:
        from lunar_python import Solar  # type: ignore
    except ImportError as exc:
        raise SystemExit(
            f"lunar_python=={REQUIRED_LUNAR_PYTHON_VERSION} is required. Run: "
            f"{INSTALL_HINT}"
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
    for value, expected in GOLDEN_DAYS.items():
        day = build_day(value)
        if day != expected:
            raise SystemExit(f"golden date mismatch: {value}: expected {expected}, got {day}")


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
    if output_path.resolve() == DEFAULT_OUTPUT.resolve() and len(package) > MAX_ALMANAC_BYTES:
        raise SystemExit(
            f"{_display_path(output_path)} is {len(package)} bytes, "
            f"exceeds budget {MAX_ALMANAC_BYTES}"
        )
    header = unpack_header(package)
    print(f"Generated {_display_path(output_path)}")
    print(f"Date range: {header.start_date}..{header.end_date}")
    print(f"Days: {header.day_count}")
    print(f"Record offset size: {header.record_offset_size}")
    print(f"Terms: {header.term_count}")
    print(f"Strings: {header.string_count}")
    print(f"Bytes: {len(package)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
