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
    def test_generator_exposes_future_cli_paths(self) -> None:
        self.assertEqual(SCRIPT_DIR.parent, gen.ROOT)
        self.assertEqual(SCRIPT_DIR.parent / "data" / "almanac.bin", gen.DEFAULT_OUTPUT)

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

    def test_package_uses_little_endian_raw_layout(self) -> None:
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

        self.assertEqual(gen.MAGIC, package[0:8])
        self.assertEqual((1).to_bytes(2, "little"), package[8:10])
        self.assertEqual((64).to_bytes(2, "little"), package[10:12])
        self.assertEqual((2).to_bytes(4, "little"), package[20:24])
        self.assertEqual((52).to_bytes(2, "little"), package[24:26])
        self.assertEqual(bytes((2, 2)), package[26:28])
        self.assertEqual((64).to_bytes(4, "little"), package[28:32])
        self.assertEqual((168).to_bytes(4, "little"), package[32:36])
        self.assertEqual((20).to_bytes(4, "little"), package[36:40])

        first_record = package[64:116]
        expected_field_indexes = (1, 0, 2, 3, 4, 5, 6, 7)
        self.assertEqual(
            b"".join(index.to_bytes(4, "little") for index in expected_field_indexes),
            first_record[0:32],
        )
        self.assertEqual(bytes((2, 1)), first_record[32:34])
        self.assertEqual((0).to_bytes(2, "little"), first_record[34:36])
        expected_list_indexes = (8, 9, 10, 0)
        self.assertEqual(
            b"".join(index.to_bytes(4, "little") for index in expected_list_indexes),
            first_record[36:52],
        )

        first_string_offsets = (
            0,
            0,
            len("腊月初一".encode("utf-8")),
            len("腊月初一己亥年 丙子月 甲子日 鼠日".encode("utf-8")),
            len("腊月初一己亥年 丙子月 甲子日 鼠日五行海中金".encode("utf-8")),
        )
        self.assertEqual(
            b"".join(offset.to_bytes(4, "little") for offset in first_string_offsets),
            package[168:188],
        )

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
        record_mutation = bytearray(gen.build_almanac_package([day]))
        header = gen.unpack_header(record_mutation)
        self.assertTrue(gen.verify_payload_crc(record_mutation, header))

        record_mutation[header.records_offset] ^= 0x01
        self.assertFalse(gen.verify_payload_crc(record_mutation, header))

        string_mutation = bytearray(gen.build_almanac_package([day]))
        header = gen.unpack_header(string_mutation)
        self.assertTrue(gen.verify_payload_crc(string_mutation, header))

        string_mutation[-1] ^= 0x01
        self.assertFalse(gen.verify_payload_crc(string_mutation, header))


if __name__ == "__main__":
    unittest.main()
