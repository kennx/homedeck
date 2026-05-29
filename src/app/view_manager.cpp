#include "app/view_manager.h"

namespace homedeck {

ViewManager::ViewManager(ViewManagerDeps deps) : deps_(std::move(deps)) {
}

void ViewManager::begin(SystemView initialView) {
  switchTo(initialView);
}

void ViewManager::switchToNextView() {
  switch (currentView_) {
    case SystemView::Almanac:
      switchTo(SystemView::Calendar);
      break;
    case SystemView::Calendar:
      switchTo(SystemView::Countdown);
      break;
    case SystemView::Countdown:
      switchTo(SystemView::Almanac);
      break;
  }
}

SystemView ViewManager::currentView() const {
  return currentView_;
}

void ViewManager::switchTo(SystemView view) {
  currentView_ = view;
  switch (view) {
    case SystemView::Almanac:
      if (deps_.renderAlmanac) {
        deps_.renderAlmanac();
      }
      break;
    case SystemView::Calendar:
      if (deps_.renderCalendar) {
        deps_.renderCalendar();
      }
      break;
    case SystemView::Countdown:
      if (deps_.renderCountdown) {
        deps_.renderCountdown();
      }
      break;
  }
}

}  // namespace homedeck
