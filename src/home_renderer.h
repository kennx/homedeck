#pragma once

#include <ctime>
#include <string>

namespace homedeck {

struct HomeCalendarData {
  std::string year;
  std::string month;
  std::string day;
  std::string weekday;
  bool isHoliday = false;
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

HomeCalendarData makeHomeCalendarData(const std::tm& localTime);

class HomeRenderer {
 public:
  void render();
  void render(const HomeCalendarData& data);
  void renderConfigPortal(const std::string& apSsid, const std::string& ipAddress);
};

}  // namespace homedeck
