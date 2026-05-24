#include "boot_controller.h"

#include <utility>

namespace homedeck {
namespace {

constexpr unsigned long kSetupShortcutHoldMs = 5000;

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
}

BootMode BootController::mode() const {
  return mode_;
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

  if (deps_.restoreSystemTimeFromRtc) {
    deps_.restoreSystemTimeFromRtc();
  }
  if (deps_.renderHome) {
    deps_.renderHome();
  }
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
    setupShortcutConsumed_ = true;
    if (deps_.setForceConfigOnNextBoot) {
      deps_.setForceConfigOnNextBoot();
    }
    if (deps_.restart) {
      deps_.restart();
    }
  }
}

}  // namespace homedeck
