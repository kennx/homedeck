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
