#pragma once

#include <ctime>
#include <string>

#include "providers/almanac_provider.h"

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

HomeCalendarData makeHomeCalendarData(const std::tm& localTime);
HomeCalendarData makeCurrentHomeCalendarData();

// Almanac cache (shared with calendar_view.cpp)
struct AlmanacHomeCache {
  bool valid = false;
  char lunarDate[32];
  char solarTerm[32];
  char ganzhi[96];
  char wuxing[48];
  char chongsha[48];
  char zhishen[48];
  char jianchu[48];
  char taishen[64];
  char yi[512];
  char ji[512];
};

struct AlmanacCalendarCache {
  bool valid = false;
  char lunarDate[32];
  char solarTerm[32];
  char festival[32];
  int nextSpecialMonth = 0;
  int nextSpecialDay = 0;
  char nextSpecialTerm[32];
  char nextSpecialFestival[32];
  int secondSpecialMonth = 0;
  int secondSpecialDay = 0;
  char secondSpecialTerm[32];
  char secondSpecialFestival[32];
};

struct AlmanacCache {
  int year = 0;
  int month = 0;
  int day = 0;
  AlmanacHomeCache home;
  AlmanacCalendarCache calendar;
};

extern AlmanacCache gAlmanacCache;

void setAlmanacCacheString(char* dest, std::size_t size, const std::string& src);
bool almanacCacheMatches(int year, int month, int day);
void prepareAlmanacCacheDate(int year, int month, int day);
void writeHomeAlmanacCache(int year, int month, int day, const AlmanacDayData& almanac);
bool applyCachedHomeAlmanac(int year, int month, int day, HomeCalendarData& data);
std::string lookupLunarFestival(const std::string& lunarDate);

#ifdef UNIT_TEST
void resetAlmanacCacheForTest();
#endif

class AlmanacView {
 public:
  void render();
  void render(const HomeCalendarData& data);
  void renderWithOffset(int dayOffset);
  void renderSleep();
  void onButtonA();  // prev day
  void onButtonB();  // next day
  void reset();      // reset offset

 private:
  int dayOffset_ = 0;
};

}  // namespace homedeck
