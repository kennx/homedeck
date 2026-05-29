#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace homedeck {

struct AlmanacDayData {
  std::string lunarDate;
  std::string solarTerm;
  std::string ganzhi;
  std::string wuxing;
  std::string chongsha;
  std::string zhishen;
  std::string jianchu;
  std::string taishen;
  std::string yi;
  std::string ji;
};

struct AlmanacLookupDate {
  int year = 0;
  int month = 0;
  int day = 0;
};

using AlmanacLookupCallback =
    std::function<bool(const AlmanacLookupDate& date, const AlmanacDayData& data)>;

class AlmanacProvider {
 public:
  bool lookup(int year, int month, int day, AlmanacDayData* out) const;
  bool lookupEach(const AlmanacLookupDate* dates, std::size_t count, const AlmanacLookupCallback& callback) const;
};

}  // namespace homedeck
