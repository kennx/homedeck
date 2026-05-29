#pragma once

#include <string>

namespace homedeck {

struct HomeCalendarData;
struct CalendarData;

class HomeRenderer {
 public:
  void render(const HomeCalendarData& data);
  void renderCalendar(const CalendarData& data);
  void renderConfigPortal(const std::string& apSsid, const std::string& ipAddress);
};

}  // namespace homedeck
