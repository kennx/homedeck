#!/usr/bin/env python3
from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass
from datetime import date, timedelta
from typing import Sequence


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
