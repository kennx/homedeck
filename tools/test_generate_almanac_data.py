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
        self.assertEqual(2, header.format_version)
        self.assertEqual(64, header.header_size)
        self.assertEqual(date(1900, 1, 1), header.start_date)
        self.assertEqual(date(1900, 1, 2), header.end_date)
        self.assertEqual(2, header.day_count)
        self.assertEqual(2, header.max_yi_count)
        self.assertEqual(2, header.max_ji_count)
        self.assertEqual(6, header.term_count)
        self.assertEqual(64, header.record_offsets_offset)
        self.assertEqual(73, header.records_offset)
        self.assertEqual(127, header.string_table_offset)
        self.assertEqual(3, header.record_offset_size)
        self.assertGreater(header.string_table_offset, header.records_offset)
        self.assertEqual("", strings[0])
        self.assertEqual(("祭祀", "祈福", "嫁娶", "出行", "安葬", "开市"), tuple(strings[1:7]))
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
        self.assertEqual((2).to_bytes(2, "little"), package[8:10])
        self.assertEqual((64).to_bytes(2, "little"), package[10:12])
        self.assertEqual((2).to_bytes(4, "little"), package[20:24])
        self.assertEqual(bytes((2, 2)), package[24:26])
        self.assertEqual((6).to_bytes(2, "little"), package[26:28])
        self.assertEqual((64).to_bytes(4, "little"), package[28:32])
        self.assertEqual((73).to_bytes(4, "little"), package[32:36])
        self.assertEqual((127).to_bytes(4, "little"), package[36:40])
        self.assertEqual((24).to_bytes(4, "little"), package[40:44])
        self.assertEqual(bytes((3,)), package[52:53])
        self.assertEqual(bytes(11), package[53:64])
        self.assertEqual(
            b"".join(offset.to_bytes(3, "little") for offset in (0, 27, 54)),
            package[64:73],
        )

        first_record = package[73:100]
        expected_field_indexes = (7, 0, 8, 9, 10, 11, 12, 13, 14, 15, 16)
        self.assertEqual(
            b"".join(index.to_bytes(2, "little") for index in expected_field_indexes),
            first_record[0:22],
        )
        self.assertEqual(bytes((2, 1)), first_record[22:24])
        self.assertEqual(bytes((0, 1, 2)), first_record[24:27])

        first_string_offsets = (
            0,
            0,
            6,
            12,
            18,
            24,
            30,
            36,
        )
        self.assertEqual(
            b"".join(offset.to_bytes(4, "little") for offset in first_string_offsets),
            package[127:159],
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

        record_mutation[header.record_offsets_offset + header.record_offset_size] ^= 0x01
        self.assertFalse(gen.verify_payload_crc(record_mutation, header))

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

    def test_require_lunar_python_returns_expected_version(self) -> None:
        self.assertEqual("1.4.8", gen.REQUIRED_LUNAR_PYTHON_VERSION)
        self.assertTrue(hasattr(gen._require_lunar_python(), "fromYmd"))

    def test_verify_golden_rejects_changed_field(self) -> None:
        original_build_day = gen.build_day

        def build_wrong_day(solar_date: date) -> gen.AlmanacDay:
            day = original_build_day(solar_date)
            if solar_date == date(2026, 12, 21):
                return gen.AlmanacDay(
                    solar_date=day.solar_date,
                    lunar_date="冬十三",
                    solar_term=day.solar_term,
                    ganzhi=day.ganzhi,
                    wuxing=day.wuxing,
                    chongsha=day.chongsha,
                    zhishen=day.zhishen,
                    jianchu=day.jianchu,
                    taishen=day.taishen,
                    yi=day.yi,
                    ji=day.ji,
                )
            return day

        try:
            gen.build_day = build_wrong_day  # type: ignore[assignment]
            with self.assertRaises(SystemExit):
                gen.verify_golden()
        finally:
            gen.build_day = original_build_day  # type: ignore[assignment]

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


if __name__ == "__main__":
    unittest.main()
