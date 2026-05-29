#pragma once

#include <ctime>
#include <string>

namespace homedeck {

struct CountdownData {
  int currentYear = 0;
  int nextYear = 0;
  int daysRemaining = 0;
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
