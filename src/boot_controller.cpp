#include "boot_controller.h"

#include <cstdint>
#include <ctime>
#include <utility>

namespace homedeck {
namespace {

constexpr unsigned long kSetupShortcutHoldMs = 5000;
constexpr unsigned long kHomeDisplayDurationMs = 300000;
constexpr std::time_t kTrustedUnixTimeThreshold = 1704067200;
constexpr std::uint64_t kMicrosPerSecond = 1000000ULL;
constexpr std::uint64_t kFallbackSleepSeconds = 3600ULL;
constexpr int kButtonCWakeupGpio = 1;
constexpr int kCalendarMonthOffsetMin = -120;
constexpr int kCalendarMonthOffsetMax = 120;
constexpr int kAlmanacDayOffsetMin = -3650;
constexpr int kAlmanacDayOffsetMax = 3650;

}  // namespace

BootController::BootController(BootControllerDeps deps) : deps_(std::move(deps)) {
}

void BootController::begin() {
  const BootFlags flags = deps_.loadFlags ? deps_.loadFlags() : BootFlags{};
  started_ = true;

  if (!flags.configured) {
    enterConfigMode();
    return;
  }

  if (flags.forceConfigOnNextBoot) {
    if (deps_.clearForceConfigOnNextBoot) {
      deps_.clearForceConfigOnNextBoot();
    }
    enterConfigMode();
    return;
  }

  enterSystemMode();
}

void BootController::update() {
  if (!started_) {
    return;
  }

  if (mode_ == BootMode::Config) {
    if (deps_.handleConfigPortalClient) {
      deps_.handleConfigPortalClient();
    }
    return;
  }

  if (deps_.updateButtons) {
    deps_.updateButtons();
  }
  const unsigned long now = deps_.millis ? deps_.millis() : 0;
  updateSetupShortcut(now);
  if (setupShortcutConsumed_) {
    return;
  }

  // 1. 检测 BtnC（视图切换 / 双击回本月）
  const int btnCClicks = deps_.getCalendarButtonClickCount ? deps_.getCalendarButtonClickCount() : 0;
  if (btnCClicks == 1) {
    if (viewManager_) {
      viewManager_->switchToNextView();
      if (viewManager_->currentView() == SystemView::Calendar) {
        calendarMonthOffset_ = 0;
      } else {
        almanacDayOffset_ = 0;
      }
      if (deps_.saveCurrentView) {
        deps_.saveCurrentView(viewManager_->currentView());
      }
      lastActivityMs_ = now;
    }
  } else if (btnCClicks >= 2) {
    if (viewManager_) {
      if (viewManager_->currentView() == SystemView::Calendar) {
        calendarMonthOffset_ = 0;
        if (deps_.renderCalendarWithOffset) {
          deps_.renderCalendarWithOffset(0);
        }
      } else if (viewManager_->currentView() == SystemView::Almanac) {
        almanacDayOffset_ = 0;
        if (deps_.renderAlmanacWithOffset) {
          deps_.renderAlmanacWithOffset(0);
        }
      }
      // CountdownView 无偏移状态，双击无需重置
      lastActivityMs_ = now;
    }
  }

  // 2. 检测日历翻页（仅在 Calendar 视图）
  if (viewManager_ && viewManager_->currentView() == SystemView::Calendar) {
    bool calendarUpdated = false;
    if (deps_.wasPrevMonthClicked && deps_.wasPrevMonthClicked()) {
      if (calendarMonthOffset_ > kCalendarMonthOffsetMin) {
        calendarMonthOffset_--;
        calendarUpdated = true;
      }
    } else if (deps_.wasNextMonthClicked && deps_.wasNextMonthClicked()) {
      if (calendarMonthOffset_ < kCalendarMonthOffsetMax) {
        calendarMonthOffset_++;
        calendarUpdated = true;
      }
    }
    if (calendarUpdated && deps_.renderCalendarWithOffset) {
      deps_.renderCalendarWithOffset(calendarMonthOffset_);
      lastActivityMs_ = now;
    }
  }

  // 3. 检测黄历翻页（仅在 Almanac 视图）
  if (viewManager_ && viewManager_->currentView() == SystemView::Almanac) {
    bool almanacUpdated = false;
    if (deps_.wasPrevMonthClicked && deps_.wasPrevMonthClicked()) {
      if (almanacDayOffset_ > kAlmanacDayOffsetMin) {
        almanacDayOffset_--;
        almanacUpdated = true;
      }
    } else if (deps_.wasNextMonthClicked && deps_.wasNextMonthClicked()) {
      if (almanacDayOffset_ < kAlmanacDayOffsetMax) {
        almanacDayOffset_++;
        almanacUpdated = true;
      }
    }
    if (almanacUpdated && deps_.renderAlmanacWithOffset) {
      deps_.renderAlmanacWithOffset(almanacDayOffset_);
      lastActivityMs_ = now;
    }
  }

  updateHomeSleep(now);
}

BootMode BootController::mode() const {
  return mode_;
}

SystemView BootController::currentView() const {
  return viewManager_ ? viewManager_->currentView() : SystemView::Almanac;
}

void BootController::enterConfigMode() {
  mode_ = BootMode::Config;
  if (deps_.startConfigPortal) {
    deps_.startConfigPortal();
  }
}

void BootController::enterSystemMode() {
  mode_ = BootMode::System;
  setupButtonsPressedSinceMs_ = 0;
  setupButtonsWerePressed_ = false;
  setupShortcutConsumed_ = false;
  homeSleepRequested_ = false;
  calendarMonthOffset_ = 0;
  almanacDayOffset_ = 0;

  if (deps_.restoreSystemTimeFromRtc) {
    deps_.restoreSystemTimeFromRtc();
  }

  ViewManagerDeps vmDeps{};
  vmDeps.renderAlmanac = deps_.renderAlmanac;
  vmDeps.renderCalendar = deps_.renderCalendar;
  vmDeps.renderCountdown = deps_.renderCountdown;
  viewManager_ = std::make_unique<ViewManager>(std::move(vmDeps));

  SystemView initialView = deps_.loadSavedView ? deps_.loadSavedView() : SystemView::Almanac;
  viewManager_->begin(initialView);

  lastActivityMs_ = deps_.millis ? deps_.millis() : 0;
}

void BootController::updateHomeSleep(unsigned long now) {
  if (homeSleepRequested_) {
    return;
  }
  if (now - lastActivityMs_ < kHomeDisplayDurationMs) {
    return;
  }

  homeSleepRequested_ = true;
  calendarMonthOffset_ = 0;
  almanacDayOffset_ = 0;
  if (deps_.preSleepRender) {
    deps_.preSleepRender(currentView());
  }
  if (deps_.enterDeepSleep) {
    deps_.enterDeepSleep(makeHomeSleepRequest());
  }
}

HomeSleepRequest BootController::makeHomeSleepRequest() const {
  HomeSleepRequest request{};
  request.wakeupGpio = kButtonCWakeupGpio;
  request.wakeOnLow = true;
  request.timerWakeupUs = kFallbackSleepSeconds * kMicrosPerSecond;

  const std::time_t now = deps_.currentTime ? deps_.currentTime() : 0;
  if (now < kTrustedUnixTimeThreshold) {
    return request;
  }

  const std::tm* local = std::localtime(&now);
  if (local == nullptr || local->tm_year < 124 || local->tm_mon < 0 || local->tm_mon > 11 || local->tm_mday < 1) {
    return request;
  }

  // 手动计算到次日 00:00:00 的秒数。
  // 不依赖 std::mktime：ESP32 newlib 的 mktime 对 POSIX TZ 字符串支持不稳定，
  // 且 M5Unified 的 setSystemTimeFromRtc() 可能破坏 TZ 环境变量。
  const int secondsUntilMidnight = (24 - local->tm_hour) * 3600 - local->tm_min * 60 - local->tm_sec;
  if (secondsUntilMidnight <= 0) {
    return request;
  }

  request.timerWakeupUs = static_cast<std::uint64_t>(secondsUntilMidnight) * kMicrosPerSecond;
  return request;
}

void BootController::updateSetupShortcut(unsigned long now) {
  if (setupShortcutConsumed_) {
    return;
  }
  const bool pressed = deps_.areSetupButtonsPressed && deps_.areSetupButtonsPressed();
  if (!pressed) {
    setupButtonsWerePressed_ = false;
    setupButtonsPressedSinceMs_ = 0;
    return;
  }

  if (!setupButtonsWerePressed_) {
    setupButtonsWerePressed_ = true;
    setupButtonsPressedSinceMs_ = now;
    return;
  }

  if (now - setupButtonsPressedSinceMs_ >= kSetupShortcutHoldMs) {
    if (deps_.setForceConfigOnNextBoot && deps_.setForceConfigOnNextBoot()) {
      setupShortcutConsumed_ = true;
    }
    if (setupShortcutConsumed_ && deps_.restart) {
      deps_.restart();
    }
  }
}

}  // namespace homedeck
