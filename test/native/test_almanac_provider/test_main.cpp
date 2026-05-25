#include <unity.h>

#include <LittleFS.h>

#include <cstdint>
#include <string>
#include <vector>

#include "../support/almanac_fixture.h"
#include "almanac_provider.h"

namespace {

constexpr const char* kAlmanacPath = "/almanac.bin";

std::uint32_t readU32(const std::vector<std::uint8_t>& data, std::size_t offset) {
  return static_cast<std::uint32_t>(data[offset]) |
         (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
         (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
         (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

void installPackage(const std::vector<std::uint8_t>& package) {
  fakeLittleFSSetFile(kAlmanacPath, package);
}

}  // namespace

void setUp() {
  fakeLittleFSReset();
}

void tearDown() {
}

void test_missing_file_returns_false_and_balances_littlefs_lifecycle() {
  homedeck::AlmanacProvider provider;
  homedeck::AlmanacDayData out{};

  TEST_ASSERT_FALSE(provider.lookup(1900, 1, 1, &out));

  TEST_ASSERT_TRUE(LittleFS.began);
  TEST_ASSERT_TRUE(LittleFS.ended);
}

void test_valid_single_day_v2_fixture_decodes_all_display_fields() {
  installPackage(homedeck::test::buildSingleDayFixturePackage());
  homedeck::AlmanacProvider provider;
  homedeck::AlmanacDayData out{};

  TEST_ASSERT_TRUE(provider.lookup(1900, 1, 1, &out));

  TEST_ASSERT_EQUAL_STRING("腊月初一", out.lunarDate.c_str());
  TEST_ASSERT_EQUAL_STRING("", out.solarTerm.c_str());
  TEST_ASSERT_EQUAL_STRING("己亥年 丙子月 甲子日 鼠日", out.ganzhi.c_str());
  TEST_ASSERT_EQUAL_STRING("五行海中金", out.wuxing.c_str());
  TEST_ASSERT_EQUAL_STRING("冲马煞南", out.chongsha.c_str());
  TEST_ASSERT_EQUAL_STRING("值神青龙", out.zhishen.c_str());
  TEST_ASSERT_EQUAL_STRING("建除建日", out.jianchu.c_str());
  TEST_ASSERT_EQUAL_STRING("胎神占门碓外东南", out.taishen.c_str());
  TEST_ASSERT_EQUAL_STRING("祭祀 祈福", out.yi.c_str());
  TEST_ASSERT_EQUAL_STRING("嫁娶", out.ji.c_str());
  TEST_ASSERT_TRUE(LittleFS.ended);
}

void test_out_of_range_dates_return_false() {
  installPackage(homedeck::test::buildSingleDayFixturePackage());
  homedeck::AlmanacProvider provider;
  homedeck::AlmanacDayData out{};

  TEST_ASSERT_FALSE(provider.lookup(1899, 12, 31, &out));
  TEST_ASSERT_FALSE(provider.lookup(1900, 1, 2, &out));
}

void test_bad_magic_returns_false() {
  auto package = homedeck::test::buildSingleDayFixturePackage();
  package[0] = 'X';
  installPackage(package);
  homedeck::AlmanacProvider provider;
  homedeck::AlmanacDayData out{};

  TEST_ASSERT_FALSE(provider.lookup(1900, 1, 1, &out));
}

void test_bad_format_version_returns_false() {
  auto package = homedeck::test::buildSingleDayFixturePackage();
  package[8] = 1;
  package[9] = 0;
  installPackage(package);
  homedeck::AlmanacProvider provider;
  homedeck::AlmanacDayData out{};

  TEST_ASSERT_FALSE(provider.lookup(1900, 1, 1, &out));
}

void test_bad_payload_crc_returns_false() {
  auto package = homedeck::test::buildSingleDayFixturePackage();
  package.back() ^= 0x01;
  installPackage(package);
  homedeck::AlmanacProvider provider;
  homedeck::AlmanacDayData out{};

  TEST_ASSERT_FALSE(provider.lookup(1900, 1, 1, &out));
}

void test_corrupted_offset_table_returns_false() {
  auto package = homedeck::test::buildSingleDayFixturePackage();
  package[64 + 3] ^= 0x01;
  installPackage(package);
  homedeck::AlmanacProvider provider;
  homedeck::AlmanacDayData out{};

  TEST_ASSERT_FALSE(provider.lookup(1900, 1, 1, &out));
}

void test_invalid_term_id_returns_false() {
  auto package = homedeck::test::buildSingleDayFixturePackage();
  const std::uint32_t recordsOffset = readU32(package, 32);
  package[recordsOffset + 24] = 255;
  homedeck::test::rewritePayloadCrc(package);
  installPackage(package);
  homedeck::AlmanacProvider provider;
  homedeck::AlmanacDayData out{};

  TEST_ASSERT_FALSE(provider.lookup(1900, 1, 1, &out));
}

void test_two_day_fixture_reads_second_record_from_offset_table() {
  homedeck::test::AlmanacFixtureDay second{
      "腊月初二",
      "",
      "己亥年 丙子月 乙丑日 牛日",
      "五行海中金",
      "冲羊煞东",
      "值神明堂",
      "建除除日",
      "胎神碓磨厕外东南",
      {"出行"},
      {"安葬", "开市"},
  };
  installPackage(homedeck::test::buildAlmanacFixturePackage(
      1900,
      1,
      1,
      {homedeck::test::singleDayFixture(), second}));
  homedeck::AlmanacProvider provider;
  homedeck::AlmanacDayData out{};

  TEST_ASSERT_TRUE(provider.lookup(1900, 1, 2, &out));

  TEST_ASSERT_EQUAL_STRING("腊月初二", out.lunarDate.c_str());
  TEST_ASSERT_EQUAL_STRING("己亥年 丙子月 乙丑日 牛日", out.ganzhi.c_str());
  TEST_ASSERT_EQUAL_STRING("出行", out.yi.c_str());
  TEST_ASSERT_EQUAL_STRING("安葬 开市", out.ji.c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_missing_file_returns_false_and_balances_littlefs_lifecycle);
  RUN_TEST(test_valid_single_day_v2_fixture_decodes_all_display_fields);
  RUN_TEST(test_out_of_range_dates_return_false);
  RUN_TEST(test_bad_magic_returns_false);
  RUN_TEST(test_bad_format_version_returns_false);
  RUN_TEST(test_bad_payload_crc_returns_false);
  RUN_TEST(test_corrupted_offset_table_returns_false);
  RUN_TEST(test_invalid_term_id_returns_false);
  RUN_TEST(test_two_day_fixture_reads_second_record_from_offset_table);
  return UNITY_END();
}
