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
  bool temperatureAvailable = false;
  float temperatureCelsius = 0.0f;
  bool humidityAvailable = false;
  float humidityPercent = 0.0f;
  std::string bottomCenterMessage;
};

struct CalendarData {
  int year = 0;
  int month = 0;        // 1-12
  int day = 0;          // 当天，用于高亮
  int todayWeekday = 0; // 0=周日
  bool temperatureAvailable = false;
  float temperatureCelsius = 0.0f;
  bool humidityAvailable = false;
  float humidityPercent = 0.0f;
};

CalendarData makeCalendarData(const std::tm& localTime);
CalendarData makeCurrentCalendarData();

HomeCalendarData makeHomeCalendarData(const std::tm& localTime);
HomeCalendarData makeCurrentHomeCalendarData();

class HomeRenderer {
 public:
  void render();
  void render(const HomeCalendarData& data);
  void renderConfigPortal(const std::string& apSsid, const std::string& ipAddress);
  void renderCalendar(const CalendarData& data);
};

}  // namespace homedeck
