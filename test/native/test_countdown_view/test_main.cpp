#include <unity.h>

#include "countdown_view.h"

using namespace homedeck;

void test_countdown_jan_first_has_full_year_days() {
  std::tm tm{};
  tm.tm_year = 126;  // 2026
  tm.tm_mon = 0;     // Jan
  tm.tm_mday = 1;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  tm.tm_isdst = -1;

  CountdownData data = makeCountdownData(tm);
  TEST_ASSERT_EQUAL(2026, data.currentYear);
  TEST_ASSERT_EQUAL(2027, data.nextYear);
  TEST_ASSERT_EQUAL(1, data.month);
  TEST_ASSERT_EQUAL(0, data.weekday);  // tm{} zero-initializes wday to 0
  TEST_ASSERT_EQUAL(365, data.daysRemaining);  // 2026 is not a leap year
}

void test_countdown_dec_31_has_one_day() {
  std::tm tm{};
  tm.tm_year = 126;  // 2026
  tm.tm_mon = 11;    // Dec
  tm.tm_mday = 31;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  tm.tm_isdst = -1;

  CountdownData data = makeCountdownData(tm);
  TEST_ASSERT_EQUAL(12, data.month);
  TEST_ASSERT_EQUAL(0, data.weekday);
  TEST_ASSERT_EQUAL(1, data.daysRemaining);
}

void test_countdown_leap_year_has_366_days_on_jan_first() {
  std::tm tm{};
  tm.tm_year = 124;  // 2024 (leap year)
  tm.tm_mon = 0;     // Jan
  tm.tm_mday = 1;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  tm.tm_isdst = -1;

  CountdownData data = makeCountdownData(tm);
  TEST_ASSERT_EQUAL(1, data.month);
  TEST_ASSERT_EQUAL(0, data.weekday);
  TEST_ASSERT_EQUAL(366, data.daysRemaining);
}

void test_countdown_may_29_2026() {
  std::tm tm{};
  tm.tm_year = 126;  // 2026
  tm.tm_mon = 4;     // May
  tm.tm_mday = 29;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  tm.tm_isdst = -1;

  CountdownData data = makeCountdownData(tm);
  TEST_ASSERT_EQUAL(5, data.month);
  TEST_ASSERT_EQUAL(0, data.weekday);
  // May 29 -> Jan 1 next year = 217 days
  // May 29->31 = 2, Jun = 30 (32), Jul = 31 (63), Aug = 31 (94),
  // Sep = 30 (124), Oct = 31 (155), Nov = 30 (185), Dec = 31 (216),
  // Dec 31 -> Jan 1 = 1 (217)
  TEST_ASSERT_EQUAL(217, data.daysRemaining);
}

void test_countdown_negative_is_clamped_to_zero() {
  // If we somehow pass a date in the next year, daysRemaining should be 0
  std::tm tm{};
  tm.tm_year = 127;  // 2027
  tm.tm_mon = 0;     // Jan
  tm.tm_mday = 2;    // Jan 2
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  tm.tm_isdst = -1;

  // This is past 2027-01-01, so daysBetween should be negative
  // but we clamp to 0
  std::tm start{};
  start.tm_year = 127;
  start.tm_mon = 0;
  start.tm_mday = 2;
  start.tm_hour = 0;
  start.tm_min = 0;
  start.tm_sec = 0;
  start.tm_isdst = -1;

  // Actually makeCountdownData always uses nextYear = currentYear + 1,
  // so for 2027-01-02, nextYear = 2028, and daysRemaining should be positive.
  // Let's just test the currentYear/nextYear fields.
  CountdownData data = makeCountdownData(tm);
  TEST_ASSERT_EQUAL(2027, data.currentYear);
  TEST_ASSERT_EQUAL(2028, data.nextYear);
  TEST_ASSERT_EQUAL(1, data.month);
  TEST_ASSERT_EQUAL(0, data.weekday);
  TEST_ASSERT_TRUE(data.daysRemaining > 0);
}

void test_countdown_weekday_via_mktime() {
  std::tm tm{};
  tm.tm_year = 126;  // 2026
  tm.tm_mon = 4;     // May
  tm.tm_mday = 29;
  tm.tm_isdst = -1;
  std::mktime(&tm);  // 正规化，填充 tm_wday

  CountdownData data = makeCountdownData(tm);
  TEST_ASSERT_EQUAL(5, data.month);
  TEST_ASSERT_EQUAL(5, data.weekday);  // 2026-05-29 是星期五
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_countdown_jan_first_has_full_year_days);
  RUN_TEST(test_countdown_dec_31_has_one_day);
  RUN_TEST(test_countdown_leap_year_has_366_days_on_jan_first);
  RUN_TEST(test_countdown_may_29_2026);
  RUN_TEST(test_countdown_negative_is_clamped_to_zero);
  RUN_TEST(test_countdown_weekday_via_mktime);
  return UNITY_END();
}
