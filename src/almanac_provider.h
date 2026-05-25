#pragma once

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

class AlmanacProvider {
 public:
  bool lookup(int year, int month, int day, AlmanacDayData* out) const;
};

}  // namespace homedeck
