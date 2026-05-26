#include "view_manager.h"

namespace homedeck {

ViewManager::ViewManager(ViewManagerDeps deps) : deps_(std::move(deps)) {
}

void ViewManager::begin() {
  switchTo(SystemView::Almanac);
}

void ViewManager::switchToNextView() {
  switch (currentView_) {
    case SystemView::Almanac:
      switchTo(SystemView::Calendar);
      break;
    case SystemView::Calendar:
      switchTo(SystemView::Almanac);
      break;
  }
}

SystemView ViewManager::currentView() const {
  return currentView_;
}

bool ViewManager::viewSwitched() const {
  return viewSwitched_;
}

void ViewManager::resetViewSwitched() {
  viewSwitched_ = false;
}

void ViewManager::switchTo(SystemView view) {
  currentView_ = view;
  viewSwitched_ = true;
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
  }
}

}  // namespace homedeck
