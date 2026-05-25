#include "almanac_provider.h"

#include <LittleFS.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace homedeck {
namespace {

constexpr const char* kAlmanacPath = "/almanac.bin";
constexpr std::array<char, 8> kMagic = {'H', 'D', 'A', 'L', 'M', '0', '0', '1'};
constexpr std::uint16_t kFormatVersion = 2;
constexpr std::uint16_t kHeaderSize = 64;
constexpr std::uint8_t kTextFieldCount = 11;
constexpr std::uint8_t kRecordOffsetSize = 3;
constexpr std::uint32_t kStringIndexSize = 4;
constexpr int kCanonicalStartYear = 1900;
constexpr std::uint8_t kCanonicalStartMonth = 1;
constexpr std::uint8_t kCanonicalStartDay = 1;
constexpr int kCanonicalEndYear = 2100;
constexpr std::uint8_t kCanonicalEndMonth = 12;
constexpr std::uint8_t kCanonicalEndDay = 31;
constexpr std::uint32_t kCanonicalDayCount = 73414;

struct AlmanacHeader {
  std::int16_t startYear = 0;
  std::uint8_t startMonth = 0;
  std::uint8_t startDay = 0;
  std::int16_t endYear = 0;
  std::uint8_t endMonth = 0;
  std::uint8_t endDay = 0;
  std::uint32_t dayCount = 0;
  std::uint8_t maxYiCount = 0;
  std::uint8_t maxJiCount = 0;
  std::uint16_t termCount = 0;
  std::uint32_t recordOffsetsOffset = 0;
  std::uint32_t recordsOffset = 0;
  std::uint32_t stringTableOffset = 0;
  std::uint32_t stringCount = 0;
  std::uint32_t stringTableSize = 0;
  std::uint32_t payloadCrc32 = 0;
  std::uint8_t recordOffsetSize = 0;
};

std::uint16_t readU16(const std::vector<std::uint8_t>& data, std::size_t offset) {
  return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}

std::int16_t readI16(const std::vector<std::uint8_t>& data, std::size_t offset) {
  return static_cast<std::int16_t>(readU16(data, offset));
}

std::uint32_t readU24(const std::vector<std::uint8_t>& data, std::size_t offset) {
  return static_cast<std::uint32_t>(data[offset]) |
         (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
         (static_cast<std::uint32_t>(data[offset + 2]) << 16);
}

std::uint32_t readU32(const std::vector<std::uint8_t>& data, std::size_t offset) {
  return static_cast<std::uint32_t>(data[offset]) |
         (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
         (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
         (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

std::uint32_t updateCrc32(std::uint32_t crc, const std::uint8_t* data, std::size_t length) {
  for (std::size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      const std::uint32_t mask = 0U - (crc & 1U);
      crc = (crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return crc;
}

bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

bool isValidDate(int year, int month, int day) {
  if (month < 1 || month > 12 || day < 1) {
    return false;
  }
  static constexpr int kDaysByMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int maxDay = kDaysByMonth[month - 1];
  if (month == 2 && isLeapYear(year)) {
    maxDay = 29;
  }
  return day <= maxDay;
}

int daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned dayOfYear =
      (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra =
      yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + static_cast<int>(dayOfEra) - 719468;
}

bool readExact(File& file, std::uint32_t offset, std::uint8_t* buffer, std::size_t length) {
  if (buffer == nullptr) {
    return false;
  }
  if (!file.seek(offset)) {
    return false;
  }
  return file.read(buffer, length) == length;
}

bool readExact(File& file, std::uint32_t offset, std::vector<std::uint8_t>& buffer, std::size_t length) {
  buffer.assign(length, 0);
  if (length == 0) {
    return true;
  }
  return readExact(file, offset, buffer.data(), length);
}

bool readHeader(File& file, AlmanacHeader& header) {
  std::vector<std::uint8_t> data;
  if (file.size() < kHeaderSize || !readExact(file, 0, data, kHeaderSize)) {
    return false;
  }
  if (!std::equal(kMagic.begin(), kMagic.end(), data.begin())) {
    return false;
  }

  header.startYear = readI16(data, 12);
  header.startMonth = data[14];
  header.startDay = data[15];
  header.endYear = readI16(data, 16);
  header.endMonth = data[18];
  header.endDay = data[19];
  header.dayCount = readU32(data, 20);
  header.maxYiCount = data[24];
  header.maxJiCount = data[25];
  header.termCount = readU16(data, 26);
  header.recordOffsetsOffset = readU32(data, 28);
  header.recordsOffset = readU32(data, 32);
  header.stringTableOffset = readU32(data, 36);
  header.stringCount = readU32(data, 40);
  header.stringTableSize = readU32(data, 44);
  header.payloadCrc32 = readU32(data, 48);
  header.recordOffsetSize = data[52];

  if (readU16(data, 8) != kFormatVersion || readU16(data, 10) != kHeaderSize) {
    return false;
  }
  if (header.recordOffsetSize != kRecordOffsetSize || header.recordOffsetsOffset != kHeaderSize) {
    return false;
  }
  if (header.dayCount == 0 || header.stringCount == 0 || header.termCount > 255 ||
      header.stringCount <= header.termCount || header.stringCount > 65535) {
    return false;
  }
  if (!std::all_of(data.begin() + 53, data.end(), [](std::uint8_t value) { return value == 0; })) {
    return false;
  }
  if (!isValidDate(header.startYear, header.startMonth, header.startDay) ||
      !isValidDate(header.endYear, header.endMonth, header.endDay)) {
    return false;
  }
  if (header.startYear != kCanonicalStartYear ||
      header.startMonth != kCanonicalStartMonth ||
      header.startDay != kCanonicalStartDay ||
      header.endYear != kCanonicalEndYear ||
      header.endMonth != kCanonicalEndMonth ||
      header.endDay != kCanonicalEndDay ||
      header.dayCount != kCanonicalDayCount) {
    return false;
  }

  const std::uint64_t expectedRecordsOffset =
      static_cast<std::uint64_t>(header.recordOffsetsOffset) +
      (static_cast<std::uint64_t>(header.dayCount) + 1U) * kRecordOffsetSize;
  const std::uint64_t stringTableEnd =
      static_cast<std::uint64_t>(header.stringTableOffset) + header.stringTableSize;
  const std::uint64_t stringOffsetBytes =
      (static_cast<std::uint64_t>(header.stringCount) + 1U) * kStringIndexSize;
  if (expectedRecordsOffset > 0xFFFFFFFFU ||
      header.recordsOffset != static_cast<std::uint32_t>(expectedRecordsOffset)) {
    return false;
  }
  if (header.recordsOffset > header.stringTableOffset || stringTableEnd > file.size() ||
      header.stringTableSize < stringOffsetBytes) {
    return false;
  }

  const int startDays = daysFromCivil(header.startYear, header.startMonth, header.startDay);
  const int endDays = daysFromCivil(header.endYear, header.endMonth, header.endDay);
  if (endDays < startDays ||
      static_cast<std::uint32_t>(endDays - startDays + 1) != header.dayCount) {
    return false;
  }
  return true;
}

bool verifyPayloadCrc(File& file, const AlmanacHeader& header) {
  const std::uint32_t payloadEnd = header.stringTableOffset + header.stringTableSize;
  std::uint32_t offset = kHeaderSize;
  std::uint32_t crc = 0xFFFFFFFF;
  std::array<std::uint8_t, 512> buffer{};
  while (offset < payloadEnd) {
    const std::size_t chunk = std::min<std::size_t>(buffer.size(), payloadEnd - offset);
    if (!readExact(file, offset, buffer.data(), chunk)) {
      return false;
    }
    crc = updateCrc32(crc, buffer.data(), chunk);
    offset += static_cast<std::uint32_t>(chunk);
  }
  return (crc ^ 0xFFFFFFFFU) == header.payloadCrc32;
}

bool validateRecordOffsetTableEnd(File& file, const AlmanacHeader& header) {
  std::vector<std::uint8_t> bytes;
  const std::uint32_t finalOffsetPosition =
      header.recordOffsetsOffset + header.dayCount * kRecordOffsetSize;
  if (!readExact(file, finalOffsetPosition, bytes, kRecordOffsetSize)) {
    return false;
  }

  const std::uint32_t finalRecordOffset = readU24(bytes, 0);
  const std::uint32_t recordsBlobLength = header.stringTableOffset - header.recordsOffset;
  return finalRecordOffset == recordsBlobLength;
}

bool dateOffset(const AlmanacHeader& header, int year, int month, int day, std::uint32_t& out) {
  if (!isValidDate(year, month, day)) {
    return false;
  }
  const int target = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
  const int start = daysFromCivil(header.startYear, header.startMonth, header.startDay);
  const int offset = target - start;
  if (offset < 0 || static_cast<std::uint32_t>(offset) >= header.dayCount) {
    return false;
  }
  out = static_cast<std::uint32_t>(offset);
  return true;
}

bool readString(File& file, const AlmanacHeader& header, std::uint32_t index, std::string& out) {
  if (index >= header.stringCount) {
    return false;
  }
  std::vector<std::uint8_t> offsetBytes;
  const std::uint32_t offsetPosition = header.stringTableOffset + index * kStringIndexSize;
  if (!readExact(file, offsetPosition, offsetBytes, 8)) {
    return false;
  }
  const std::uint32_t start = readU32(offsetBytes, 0);
  const std::uint32_t end = readU32(offsetBytes, 4);
  const std::uint32_t offsetsSize = (header.stringCount + 1U) * kStringIndexSize;
  const std::uint32_t blobSize = header.stringTableSize - offsetsSize;
  if (start > end || end > blobSize) {
    return false;
  }

  std::vector<std::uint8_t> bytes;
  const std::uint32_t blobStart = header.stringTableOffset + offsetsSize;
  if (!readExact(file, blobStart + start, bytes, end - start)) {
    return false;
  }
  out.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  return true;
}

bool readRecordOffsets(
    File& file,
    const AlmanacHeader& header,
    std::uint32_t dayOffset,
    std::uint32_t& start,
    std::uint32_t& end) {
  std::vector<std::uint8_t> bytes;
  const std::uint32_t offset = header.recordOffsetsOffset + dayOffset * kRecordOffsetSize;
  if (!readExact(file, offset, bytes, kRecordOffsetSize * 2)) {
    return false;
  }
  start = readU24(bytes, 0);
  end = readU24(bytes, 3);
  if (start > end) {
    return false;
  }
  const std::uint64_t recordEnd = static_cast<std::uint64_t>(header.recordsOffset) + end;
  return recordEnd <= header.stringTableOffset;
}

std::string joinTerms(const std::vector<std::string>& terms) {
  std::string joined;
  for (const auto& term : terms) {
    if (!joined.empty()) {
      joined += ' ';
    }
    joined += term;
  }
  return joined;
}

bool decodeTerms(
    File& file,
    const AlmanacHeader& header,
    const std::vector<std::uint8_t>& record,
    std::size_t offset,
    std::uint8_t count,
    std::vector<std::string>& terms) {
  terms.clear();
  for (std::uint8_t i = 0; i < count; ++i) {
    const std::uint8_t termId = record[offset + i];
    if (termId >= header.termCount) {
      return false;
    }
    std::string term;
    if (!readString(file, header, static_cast<std::uint32_t>(termId) + 1U, term)) {
      return false;
    }
    terms.push_back(term);
  }
  return true;
}

bool decodeRecord(
    File& file,
    const AlmanacHeader& header,
    std::uint32_t recordStart,
    std::uint32_t recordEnd,
    AlmanacDayData& out) {
  const std::uint32_t recordLength = recordEnd - recordStart;
  constexpr std::uint32_t kMinimumRecordLength = kTextFieldCount * 2U + 2U;
  if (recordLength < kMinimumRecordLength) {
    return false;
  }
  const std::uint32_t maximumRecordLength =
      kMinimumRecordLength + header.maxYiCount + header.maxJiCount;
  if (recordLength > maximumRecordLength) {
    return false;
  }

  std::vector<std::uint8_t> record;
  if (!readExact(file, header.recordsOffset + recordStart, record, recordLength)) {
    return false;
  }
  const std::uint8_t yiCount = record[kTextFieldCount * 2U];
  const std::uint8_t jiCount = record[kTextFieldCount * 2U + 1U];
  const std::uint32_t expectedLength = kMinimumRecordLength + yiCount + jiCount;
  if (yiCount > header.maxYiCount || jiCount > header.maxJiCount || expectedLength != recordLength) {
    return false;
  }

  std::array<std::string, kTextFieldCount> fields;
  for (std::uint8_t i = 0; i < kTextFieldCount; ++i) {
    if (!readString(file, header, readU16(record, i * 2U), fields[i])) {
      return false;
    }
  }

  std::vector<std::string> yi;
  std::vector<std::string> ji;
  const std::size_t yiOffset = kMinimumRecordLength;
  const std::size_t jiOffset = yiOffset + yiCount;
  if (!decodeTerms(file, header, record, yiOffset, yiCount, yi) ||
      !decodeTerms(file, header, record, jiOffset, jiCount, ji)) {
    return false;
  }

  out.lunarDate = fields[0];
  out.solarTerm = fields[1];
  out.ganzhi = fields[2] + "年 " + fields[3] + "月 " + fields[4] + "日 " + fields[5] + "日";
  out.wuxing = fields[6];
  out.chongsha = fields[7];
  out.zhishen = fields[8];
  out.jianchu = fields[9];
  out.taishen = fields[10];
  out.yi = joinTerms(yi);
  out.ji = joinTerms(ji);
  return true;
}

bool lookupInFile(File& file, int year, int month, int day, AlmanacDayData& out) {
  AlmanacHeader header;
  if (!readHeader(file, header) || !verifyPayloadCrc(file, header) ||
      !validateRecordOffsetTableEnd(file, header)) {
    return false;
  }

  std::uint32_t dayOffset = 0;
  if (!dateOffset(header, year, month, day, dayOffset)) {
    return false;
  }

  std::uint32_t recordStart = 0;
  std::uint32_t recordEnd = 0;
  if (!readRecordOffsets(file, header, dayOffset, recordStart, recordEnd)) {
    return false;
  }
  return decodeRecord(file, header, recordStart, recordEnd, out);
}

}  // namespace

bool AlmanacProvider::lookup(int year, int month, int day, AlmanacDayData* out) const {
  if (out == nullptr) {
    return false;
  }

  if (!LittleFS.begin()) {
    LittleFS.end();
    return false;
  }

  File file = LittleFS.open(kAlmanacPath, "r");
  bool result = false;
  if (file) {
    result = lookupInFile(file, year, month, day, *out);
    file.close();
  }
  LittleFS.end();
  return result;
}

}  // namespace homedeck
