#pragma once

#include <functional>

namespace homedeck {

enum class SystemView {
  Almanac,
  Calendar,
};

struct ViewManagerDeps {
  std::function<void()> renderAlmanac;
  std::function<void()> renderCalendar;
};

class ViewManager {
 public:
  explicit ViewManager(ViewManagerDeps deps);
  void begin();
  void switchToNextView();
  SystemView currentView() const;
  bool viewSwitched() const;
  void resetViewSwitched();

 private:
  void switchTo(SystemView view);

  ViewManagerDeps deps_;
  SystemView currentView_ = SystemView::Almanac;
  bool viewSwitched_ = false;
};

}  // namespace homedeck
