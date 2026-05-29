#pragma once

#include <functional>

namespace homedeck {

enum class SystemView {
  Almanac,
  Calendar,
  Countdown,
};

struct ViewManagerDeps {
  std::function<void()> renderAlmanac;
  std::function<void()> renderCalendar;
  std::function<void()> renderCountdown;
};

class ViewManager {
 public:
  explicit ViewManager(ViewManagerDeps deps);
  void begin(SystemView initialView = SystemView::Almanac);
  void switchToNextView();
  SystemView currentView() const;

 private:
  void switchTo(SystemView view);

  ViewManagerDeps deps_;
  SystemView currentView_ = SystemView::Almanac;
};

}  // namespace homedeck
