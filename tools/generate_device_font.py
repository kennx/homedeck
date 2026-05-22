#!/usr/bin/env python3
from __future__ import annotations

import os
import shlex
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_FONT = ROOT / "fonts" / "NotoSansSC-Regular.ttf"
PIXEL_SIZE = 14
MIN_GLYPH_COUNT = 6000
BUILD_DIR = ROOT / ".pio" / "build" / "font-tools"
CODEPOINTS_PATH = BUILD_DIR / "device_font_codepoints.txt"
ENCODER_PATH = BUILD_DIR / "font_to_vlw"
VLW_PATH = BUILD_DIR / "device_font.vlw"
HEADER_PATH = ROOT / "src" / "generated" / "device_font_vlw.h"
SOURCE_PATH = ROOT / "src" / "generated" / "device_font_vlw.cpp"
EXTRA_TEXT = (
    "，。！？；：“”‘’（）《》、—…·"
    "℃°％%年月日星期农历节气节假日今日日程温度湿度还有项连接开放热点打开当前热点配置不可用为空同步网络传感器"
    "HomeDeck Wi-Fi NTP webcal"
)
SCAN_DIRS = (
    ROOT / "src",
    ROOT / "lib" / "homedeck_core" / "src",
    ROOT / "test" / "native",
)
SCAN_SUFFIXES = {".cpp", ".h", ".hpp", ".ino"}


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


def write_codepoints(codepoints: list[int]) -> None:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    with CODEPOINTS_PATH.open("w", encoding="utf-8", newline="\n") as output:
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


def run_encoder() -> None:
    run(
        [
            str(ENCODER_PATH),
            str(SOURCE_FONT),
            str(CODEPOINTS_PATH),
            str(PIXEL_SIZE),
            str(VLW_PATH),
        ]
    )


def write_header(vlw_size: int, glyph_count: int) -> None:
    HEADER_PATH.parent.mkdir(parents=True, exist_ok=True)
    content = f"""#pragma once

#include <cstddef>
#include <cstdint>

namespace homedeck::generated {{

extern const std::uint8_t kDeviceFontVlw[];
inline constexpr std::size_t kDeviceFontVlwSize = {vlw_size}U;
inline constexpr std::uint32_t kDeviceFontGlyphCount = {glyph_count}U;
inline constexpr std::uint32_t kDeviceFontPixelSize = {PIXEL_SIZE}U;

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


def write_source(vlw_data: bytes) -> None:
    SOURCE_PATH.parent.mkdir(parents=True, exist_ok=True)
    content = f"""#include "device_font_vlw.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

#ifndef PROGMEM
#define PROGMEM
#endif

namespace homedeck::generated {{

const std::uint8_t kDeviceFontVlw[] PROGMEM = {{
{format_byte_array(vlw_data)}
}};

static_assert(sizeof(kDeviceFontVlw) == kDeviceFontVlwSize,
              "device font VLW size metadata mismatch");

}}  // namespace homedeck::generated
"""
    with SOURCE_PATH.open("w", encoding="utf-8", newline="\n") as output:
        output.write(content)


def main() -> None:
    if not SOURCE_FONT.exists():
        raise SystemExit(f"source font missing: {SOURCE_FONT}")

    codepoints = collect_codepoints()
    if len(codepoints) < MIN_GLYPH_COUNT:
        raise SystemExit(
            f"collected glyph set is unexpectedly small: {len(codepoints)}"
        )

    write_codepoints(codepoints)
    compile_encoder()
    run_encoder()

    vlw_data = VLW_PATH.read_bytes()
    write_header(len(vlw_data), len(codepoints))
    write_source(vlw_data)

    print(f"Generated {CODEPOINTS_PATH.relative_to(ROOT)}")
    print(f"Generated {VLW_PATH.relative_to(ROOT)}")
    print(f"Generated {HEADER_PATH.relative_to(ROOT)}")
    print(f"Generated {SOURCE_PATH.relative_to(ROOT)}")
    print(f"Glyphs: {len(codepoints)}")
    print(f"VLW bytes: {len(vlw_data)}")


if __name__ == "__main__":
    main()
