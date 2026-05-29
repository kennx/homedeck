#pragma once

#include <ctime>
#include <string>

namespace homedeck {

struct CalendarData {
  int year = 0;
  int month = 0;        // 1-12
  int day = 0;          // 当天，用于高亮
  int todayWeekday = 0; // 0=周日
  bool temperatureAvailable = false;
  float temperatureCelsius = 0.0f;
  bool humidityAvailable = false;
  float humidityPercent = 0.0f;
  std::string bottomCenterMessage;
  std::string lunarDate;
  std::string solarTerm;
  std::string festival;
  int nextSpecialMonth = 0;
  int nextSpecialDay = 0;
  std::string nextSpecialTerm;
  std::string nextSpecialFestival;
  int secondSpecialMonth = 0;
  int secondSpecialDay = 0;
  std::string secondSpecialTerm;
  std::string secondSpecialFestival;
  int todayMonth = 0;
  int todayDay = 0;
};

CalendarData makeCalendarData(const std::tm& localTime);
CalendarData makeCurrentCalendarData();
void applySht40ToCalendar(CalendarData& data);

class CalendarView {
 public:
  void render();
  void render(const CalendarData& data);
  void renderWithOffset(int monthOffset);
  void renderSleep();
  void onButtonA();  // prev month
  void onButtonB();  // next month
  void reset();      // reset offset

 private:
  int monthOffset_ = 0;
};

}  // namespace homedeck
