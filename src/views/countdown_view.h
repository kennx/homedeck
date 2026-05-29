#pragma once

#include <ctime>
#include <string>

namespace homedeck {

struct CountdownData {
  int currentYear = 0;
  int nextYear = 0;
  int daysRemaining = 0;
  int month = 0;     // 1-12
  int weekday = 0;   // 0=周日
  bool temperatureAvailable = false;
  float temperatureCelsius = 0.0f;
  bool humidityAvailable = false;
  float humidityPercent = 0.0f;
  std::string bottomCenterMessage;
};

CountdownData makeCountdownData(const std::tm& localTime);
CountdownData makeCurrentCountdownData();

class CountdownView {
 public:
  void render();
  void render(const CountdownData& data);
  void renderSleep();
};

}  // namespace homedeck
