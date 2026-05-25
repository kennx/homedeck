#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace homedeck::test {

constexpr int kCanonicalStartYear = 1900;
constexpr int kCanonicalStartMonth = 1;
constexpr int kCanonicalStartDay = 1;
constexpr int kCanonicalEndYear = 2100;
constexpr int kCanonicalEndMonth = 12;
constexpr int kCanonicalEndDay = 31;
constexpr std::uint32_t kCanonicalDayCount = 73414;

struct AlmanacFixtureDay {
  std::string lunarDate;
  std::string solarTerm;
  std::string ganzhi;
  std::string wuxing;
  std::string chongsha;
  std::string zhishen;
  std::string jianchu;
  std::string taishen;
  std::vector<std::string> yi;
  std::vector<std::string> ji;
};

inline void pushU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

inline void pushI16(std::vector<std::uint8_t>& out, std::int16_t value) {
  pushU16(out, static_cast<std::uint16_t>(value));
}

inline void pushU24(std::vector<std::uint8_t>& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
}

inline void pushU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

inline std::uint16_t readU16(const std::vector<std::uint8_t>& data, std::size_t offset) {
  return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}

inline std::uint32_t readU32(const std::vector<std::uint8_t>& data, std::size_t offset) {
  return static_cast<std::uint32_t>(data[offset]) |
         (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
         (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
         (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

inline void writeU32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value) {
  data[offset] = static_cast<std::uint8_t>(value & 0xFF);
  data[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
  data[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
  data[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
}

inline std::uint32_t crc32(const std::uint8_t* data, std::size_t length) {
  std::uint32_t crc = 0xFFFFFFFF;
  for (std::size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      const std::uint32_t mask = 0U - (crc & 1U);
      crc = (crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return crc ^ 0xFFFFFFFF;
}

inline std::vector<std::string> splitGanzhi(const std::string& ganzhi) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= ganzhi.size()) {
    const std::size_t end = ganzhi.find(' ', start);
    parts.push_back(ganzhi.substr(start, end == std::string::npos ? std::string::npos : end - start));
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  if (parts.size() != 4) {
    throw std::runtime_error("unexpected ganzhi format");
  }
  const std::vector<std::string> suffixes = {"年", "月", "日", "日"};
  for (std::size_t i = 0; i < parts.size(); ++i) {
    const auto& suffix = suffixes[i];
    if (parts[i].size() < suffix.size() ||
        parts[i].compare(parts[i].size() - suffix.size(), suffix.size(), suffix) != 0) {
      throw std::runtime_error("unexpected ganzhi suffix");
    }
    parts[i].erase(parts[i].size() - suffix.size());
  }
  return parts;
}

inline bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

inline int daysInMonth(int year, int month) {
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && isLeapYear(year) ? 29 : kDays[month - 1];
}

inline void addDays(int& year, int& month, int& day, std::size_t delta) {
  for (std::size_t i = 0; i < delta; ++i) {
    ++day;
    if (day <= daysInMonth(year, month)) {
      continue;
    }
    day = 1;
    ++month;
    if (month <= 12) {
      continue;
    }
    month = 1;
    ++year;
  }
}

inline std::size_t daysFromCanonicalStart(int year, int month, int day) {
  int currentYear = kCanonicalStartYear;
  int currentMonth = kCanonicalStartMonth;
  int currentDay = kCanonicalStartDay;
  for (std::size_t offset = 0; offset < kCanonicalDayCount; ++offset) {
    if (currentYear == year && currentMonth == month && currentDay == day) {
      return offset;
    }
    addDays(currentYear, currentMonth, currentDay, 1);
  }
  throw std::runtime_error("fixture date is outside canonical almanac range");
}

inline std::vector<std::string> textFields(const AlmanacFixtureDay& day) {
  auto parts = splitGanzhi(day.ganzhi);
  return {
      day.lunarDate,
      day.solarTerm,
      parts[0],
      parts[1],
      parts[2],
      parts[3],
      day.wuxing,
      day.chongsha,
      day.zhishen,
      day.jianchu,
      day.taishen,
  };
}

inline void addString(
    const std::string& value,
    std::vector<std::string>& strings,
    std::map<std::string, std::uint16_t>& indexes) {
  if (indexes.find(value) != indexes.end()) {
    return;
  }
  indexes[value] = static_cast<std::uint16_t>(strings.size());
  strings.push_back(value);
}

inline std::vector<std::uint8_t> buildAlmanacFixturePackage(
    int startYear,
    int startMonth,
    int startDay,
    const std::vector<AlmanacFixtureDay>& days) {
  if (days.empty()) {
    throw std::runtime_error("at least one fixture day is required");
  }

  std::vector<std::string> terms;
  std::map<std::string, std::uint8_t> termIds;
  for (const auto& day : days) {
    for (const auto& value : day.yi) {
      if (termIds.find(value) == termIds.end()) {
        termIds[value] = static_cast<std::uint8_t>(terms.size());
        terms.push_back(value);
      }
    }
    for (const auto& value : day.ji) {
      if (termIds.find(value) == termIds.end()) {
        termIds[value] = static_cast<std::uint8_t>(terms.size());
        terms.push_back(value);
      }
    }
  }

  std::vector<std::string> strings{""};
  std::map<std::string, std::uint16_t> stringIndexes{{"", 0}};
  for (const auto& term : terms) {
    addString(term, strings, stringIndexes);
  }
  for (const auto& day : days) {
    for (const auto& value : textFields(day)) {
      addString(value, strings, stringIndexes);
    }
  }

  std::vector<std::uint8_t> records;
  const std::size_t startOffset = daysFromCanonicalStart(startYear, startMonth, startDay);
  if (startOffset + days.size() > kCanonicalDayCount) {
    throw std::runtime_error("fixture days exceed canonical almanac range");
  }
  std::vector<std::uint32_t> recordOffsets(startOffset + 1, 0);
  std::uint8_t maxYiCount = 0;
  std::uint8_t maxJiCount = 0;
  for (const auto& day : days) {
    maxYiCount = std::max(maxYiCount, static_cast<std::uint8_t>(day.yi.size()));
    maxJiCount = std::max(maxJiCount, static_cast<std::uint8_t>(day.ji.size()));
    for (const auto& value : textFields(day)) {
      pushU16(records, stringIndexes[value]);
    }
    records.push_back(static_cast<std::uint8_t>(day.yi.size()));
    records.push_back(static_cast<std::uint8_t>(day.ji.size()));
    for (const auto& value : day.yi) {
      records.push_back(termIds[value]);
    }
    for (const auto& value : day.ji) {
      records.push_back(termIds[value]);
    }
    recordOffsets.push_back(static_cast<std::uint32_t>(records.size()));
  }
  recordOffsets.resize(kCanonicalDayCount + 1, static_cast<std::uint32_t>(records.size()));

  std::vector<std::uint8_t> recordOffsetsBlob;
  for (const auto offset : recordOffsets) {
    pushU24(recordOffsetsBlob, offset);
  }

  std::vector<std::uint8_t> stringBlob;
  std::vector<std::uint32_t> stringOffsets;
  for (const auto& value : strings) {
    stringOffsets.push_back(static_cast<std::uint32_t>(stringBlob.size()));
    stringBlob.insert(stringBlob.end(), value.begin(), value.end());
  }
  stringOffsets.push_back(static_cast<std::uint32_t>(stringBlob.size()));

  std::vector<std::uint8_t> stringTable;
  for (const auto offset : stringOffsets) {
    pushU32(stringTable, offset);
  }
  stringTable.insert(stringTable.end(), stringBlob.begin(), stringBlob.end());

  constexpr std::uint32_t kHeaderSize = 64;
  constexpr std::uint8_t kRecordOffsetSize = 3;
  const std::uint32_t recordOffsetsOffset = kHeaderSize;
  const std::uint32_t recordsOffset = recordOffsetsOffset + static_cast<std::uint32_t>(recordOffsetsBlob.size());
  const std::uint32_t stringTableOffset = recordsOffset + static_cast<std::uint32_t>(records.size());
  std::vector<std::uint8_t> payload;
  payload.insert(payload.end(), recordOffsetsBlob.begin(), recordOffsetsBlob.end());
  payload.insert(payload.end(), records.begin(), records.end());
  payload.insert(payload.end(), stringTable.begin(), stringTable.end());
  const std::uint32_t payloadCrc = crc32(payload.data(), payload.size());

  std::vector<std::uint8_t> package;
  const char magic[] = "HDALM001";
  package.insert(package.end(), magic, magic + 8);
  pushU16(package, 2);
  pushU16(package, kHeaderSize);
  pushI16(package, static_cast<std::int16_t>(kCanonicalStartYear));
  package.push_back(static_cast<std::uint8_t>(kCanonicalStartMonth));
  package.push_back(static_cast<std::uint8_t>(kCanonicalStartDay));
  pushI16(package, static_cast<std::int16_t>(kCanonicalEndYear));
  package.push_back(static_cast<std::uint8_t>(kCanonicalEndMonth));
  package.push_back(static_cast<std::uint8_t>(kCanonicalEndDay));
  pushU32(package, kCanonicalDayCount);
  package.push_back(maxYiCount);
  package.push_back(maxJiCount);
  pushU16(package, static_cast<std::uint16_t>(terms.size()));
  pushU32(package, recordOffsetsOffset);
  pushU32(package, recordsOffset);
  pushU32(package, stringTableOffset);
  pushU32(package, static_cast<std::uint32_t>(strings.size()));
  pushU32(package, static_cast<std::uint32_t>(stringTable.size()));
  pushU32(package, payloadCrc);
  package.push_back(kRecordOffsetSize);
  package.resize(kHeaderSize, 0);
  package.insert(package.end(), payload.begin(), payload.end());
  return package;
}

inline void rewritePayloadCrc(std::vector<std::uint8_t>& package) {
  const std::uint32_t stringTableOffset = readU32(package, 36);
  const std::uint32_t stringTableSize = readU32(package, 44);
  const std::size_t payloadEnd = static_cast<std::size_t>(stringTableOffset + stringTableSize);
  writeU32(package, 48, crc32(package.data() + 64, payloadEnd - 64));
}

inline AlmanacFixtureDay singleDayFixture() {
  return {
      "腊月初一",
      "",
      "己亥年 丙子月 甲子日 鼠日",
      "五行海中金",
      "冲马煞南",
      "值神青龙",
      "建除建日",
      "胎神占门碓外东南",
      {"祭祀", "祈福"},
      {"嫁娶"},
  };
}

inline std::vector<std::uint8_t> buildSingleDayFixturePackage() {
  return buildAlmanacFixturePackage(1900, 1, 1, {singleDayFixture()});
}

}  // namespace homedeck::test
