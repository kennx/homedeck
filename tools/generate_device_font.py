#!/usr/bin/env python3
from __future__ import annotations

import os
import shlex
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BODY_FONT = ROOT / "fonts" / "misans" / "MiSans-Semibold.ttf"
METRIC_FONT = ROOT / "fonts" / "misans" / "MiSans-Bold.ttf"
TIME_FONT = ROOT / "fonts" / "misans" / "MiSans-Heavy.ttf"
CONFIG_FONT = ROOT / "fonts" / "misans" / "MiSans-Semibold.ttf"
BODY_PIXEL_SIZE = 20
METRIC_PIXEL_SIZE = 28
TIME_PIXEL_SIZE = 42
CONFIG_PIXEL_SIZE = 20
MIN_GLYPH_COUNT = 6000
BUILD_DIR = ROOT / ".pio" / "build" / "font-tools"
ENCODER_PATH = BUILD_DIR / "font_to_vlw"
HEADER_PATH = ROOT / "src" / "generated" / "device_font_vlw.h"
SOURCE_PATH = ROOT / "src" / "generated" / "device_font_vlw.cpp"
EXTRA_TEXT = (
    "，。！？；：“”‘’（）《》、—…·"
    "℃°％%年月日星期农历节气节假日今日日程温度湿度还有项连接开放热点打开当前热点配置不可用为空同步网络传感器"
    "HomeDeck Wi-Fi NTP webcal"
)
NUMERIC_TEXT = "0123456789:-+.°℃C% "
SCAN_DIRS = (
    ROOT / "src",
    ROOT / "lib" / "homedeck_core" / "src",
    ROOT / "test" / "native",
)
SCAN_SUFFIXES = {".cpp", ".h", ".hpp", ".ino"}


class FontResource:
    def __init__(
        self,
        name: str,
        symbol_prefix: str,
        pixel_size: int,
        codepoints: list[int],
        ttf_path: Path,
    ) -> None:
        self.name = name
        self.symbol_prefix = symbol_prefix
        self.pixel_size = pixel_size
        self.codepoints = codepoints
        self.ttf_path = ttf_path
        self.codepoints_path = BUILD_DIR / f"{name}_codepoints.txt"
        self.vlw_path = BUILD_DIR / f"{name}.vlw"
        self.vlw_data = b""
        self.glyph_count = 0


def run(command: list[str]) -> None:
    print("$ " + shlex.join(command))
    subprocess.run(command, cwd=ROOT, check=True)


def collect_ascii(codepoints: set[int]) -> None:
    codepoints.update(range(0x20, 0x7F))


def collect_gb2312(codepoints: set[int]) -> None:
    for lead in range(0xA1, 0xF8):
        for trail in range(0xA1, 0xFF):
            try:
                decoded = bytes((lead, trail)).decode("gb2312")
            except UnicodeDecodeError:
                continue
            for char in decoded:
                value = ord(char)
                if value <= 0xFFFF:
                    codepoints.add(value)


def collect_text(codepoints: set[int], text: str, *, non_ascii_only: bool) -> None:
    for char in text:
        value = ord(char)
        if value <= 0xFFFF and (not non_ascii_only or value > 0x7F):
            codepoints.add(value)


def collect_project_text(codepoints: set[int]) -> None:
    for directory in SCAN_DIRS:
        if not directory.exists():
            continue
        for path in directory.rglob("*"):
            if not path.is_file() or path.suffix not in SCAN_SUFFIXES:
                continue
            collect_text(
                codepoints,
                path.read_text(encoding="utf-8", errors="ignore"),
                non_ascii_only=True,
            )


def collect_codepoints() -> list[int]:
    codepoints: set[int] = set()
    collect_ascii(codepoints)
    collect_gb2312(codepoints)
    collect_text(codepoints, EXTRA_TEXT, non_ascii_only=False)
    collect_project_text(codepoints)
    return sorted(codepoints)


def write_codepoints(path: Path, codepoints: list[int]) -> None:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as output:
        output.write("# HomeDeck generated device font code points.\n")
        for value in codepoints:
            output.write(f"{value:04X}\n")


def freetype_flags(flag: str) -> list[str]:
    commands = (
        ["freetype-config", flag],
        ["pkg-config", flag, "freetype2"],
    )
    for command in commands:
        try:
            output = subprocess.check_output(command, text=True)
        except (FileNotFoundError, subprocess.CalledProcessError):
            continue
        return shlex.split(output.strip())
    raise SystemExit("FreeType development flags not found")


def compile_encoder() -> None:
    cflags = freetype_flags("--cflags")
    libs = freetype_flags("--libs")
    command = [
        os.environ.get("CXX", "c++"),
        "-std=c++17",
        "-O2",
        *cflags,
        str(ROOT / "tools" / "font_to_vlw.cpp"),
        "-o",
        str(ENCODER_PATH),
        *libs,
    ]
    run(command)


def run_encoder(resource: FontResource) -> None:
    run(
        [
            str(ENCODER_PATH),
            str(resource.ttf_path),
            str(resource.codepoints_path),
            str(resource.pixel_size),
            str(resource.vlw_path),
        ]
    )


def read_vlw_glyph_count(data: bytes) -> int:
    if len(data) < 4:
        raise ValueError("VLW data is too short to contain a glyph count")
    return int.from_bytes(data[:4], byteorder="big", signed=False)


def write_header(resources: list[FontResource]) -> None:
    HEADER_PATH.parent.mkdir(parents=True, exist_ok=True)
    declarations = []
    for resource in resources:
        declarations.append(
            f"""extern const std::uint8_t {resource.symbol_prefix}FontVlw[];
inline constexpr std::size_t {resource.symbol_prefix}FontVlwSize = {len(resource.vlw_data)}U;
inline constexpr std::uint32_t {resource.symbol_prefix}FontGlyphCount = {resource.glyph_count}U;
inline constexpr std::uint32_t {resource.symbol_prefix}FontPixelSize = {resource.pixel_size}U;"""
        )
    declarations_text = "\n\n".join(declarations)
    content = f"""#pragma once

#include <cstddef>
#include <cstdint>

namespace homedeck::generated {{

{declarations_text}

}}  // namespace homedeck::generated
"""
    with HEADER_PATH.open("w", encoding="utf-8", newline="\n") as output:
        output.write(content)


def format_byte_array(data: bytes) -> str:
    lines: list[str] = []
    for index in range(0, len(data), 12):
        chunk = data[index : index + 12]
        values = ", ".join(f"0x{value:02X}" for value in chunk)
        lines.append(f"  {values},")
    return "\n".join(lines)


def write_source(resources: list[FontResource]) -> None:
    SOURCE_PATH.parent.mkdir(parents=True, exist_ok=True)
    arrays = []
    for resource in resources:
        arrays.append(
            f"""const std::uint8_t {resource.symbol_prefix}FontVlw[] PROGMEM = {{
{format_byte_array(resource.vlw_data)}
}};

static_assert(sizeof({resource.symbol_prefix}FontVlw) == {resource.symbol_prefix}FontVlwSize,
              "{resource.symbol_prefix}FontVlw size metadata mismatch");"""
        )
    arrays_text = "\n\n".join(arrays)
    content = f"""#include "device_font_vlw.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

#ifndef PROGMEM
#define PROGMEM
#endif

namespace homedeck::generated {{

{arrays_text}

}}  // namespace homedeck::generated
"""
    with SOURCE_PATH.open("w", encoding="utf-8", newline="\n") as output:
        output.write(content)


def main() -> None:
    for font_path in (BODY_FONT, METRIC_FONT, TIME_FONT, CONFIG_FONT):
        if not font_path.exists():
            raise SystemExit(f"source font missing: {font_path}")

    body_codepoints = collect_codepoints()
    if len(body_codepoints) < MIN_GLYPH_COUNT:
        raise SystemExit(
            f"collected glyph set is unexpectedly small: {len(body_codepoints)}"
        )

    numeric_codepoints: set[int] = set()
    collect_text(numeric_codepoints, NUMERIC_TEXT, non_ascii_only=False)

    config_codepoints: set[int] = set()
    collect_ascii(config_codepoints)

    resources = [
        FontResource("device_font", "kDevice", BODY_PIXEL_SIZE, body_codepoints, BODY_FONT),
        FontResource(
            "device_large_date_font",
            "kDeviceLargeDate",
            78,
            sorted(numeric_codepoints),
            TIME_FONT,
        ),
        FontResource(
            "device_metric_font",
            "kDeviceMetric",
            METRIC_PIXEL_SIZE,
            sorted(numeric_codepoints),
            METRIC_FONT,
        ),
        FontResource(
            "device_time_font",
            "kDeviceTime",
            TIME_PIXEL_SIZE,
            sorted(numeric_codepoints),
            TIME_FONT,
        ),
        FontResource(
            "config_portal_font",
            "kConfigPortal",
            CONFIG_PIXEL_SIZE,
            sorted(config_codepoints),
            CONFIG_FONT,
        ),
    ]

    for resource in resources:
        write_codepoints(resource.codepoints_path, resource.codepoints)

    compile_encoder()

    for resource in resources:
        run_encoder(resource)
        resource.vlw_data = resource.vlw_path.read_bytes()
        resource.glyph_count = read_vlw_glyph_count(resource.vlw_data)

    write_header(resources)
    write_source(resources)

    for resource in resources:
        print(f"Generated {resource.codepoints_path.relative_to(ROOT)}")
        print(f"Generated {resource.vlw_path.relative_to(ROOT)}")
    print(f"Generated {HEADER_PATH.relative_to(ROOT)}")
    print(f"Generated {SOURCE_PATH.relative_to(ROOT)}")
    for resource in resources:
        print(
            f"{resource.name}: {len(resource.codepoints)} glyphs, "
            f"{len(resource.vlw_data)} VLW bytes"
        )


if __name__ == "__main__":
    main()
