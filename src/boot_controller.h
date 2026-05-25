#pragma once

#include <cstdint>
#include <ctime>
#include <functional>

namespace homedeck {

struct HomeSleepRequest {
  std::uint64_t timerWakeupUs = 0;
  int wakeupGpio = 1;
  bool wakeOnLow = true;
};

enum class BootMode {
  Config,
  System,
};

struct BootFlags {
  bool configured = false;
  bool forceConfigOnNextBoot = false;
};

struct BootControllerDeps {
  std::function<BootFlags()> loadFlags;
  std::function<bool()> clearForceConfigOnNextBoot;
  std::function<bool()> setForceConfigOnNextBoot;
  std::function<void()> startConfigPortal;
  std::function<void()> handleConfigPortalClient;
  std::function<void()> restoreSystemTimeFromRtc;
  std::function<void()> renderHome;
  std::function<void()> updateButtons;
  std::function<bool()> areSetupButtonsPressed;
  std::function<unsigned long()> millis;
  std::function<void()> restart;
  std::function<std::time_t()> currentTime;
  std::function<void(const HomeSleepRequest&)> enterDeepSleep;
};

class BootController {
 public:
  explicit BootController(BootControllerDeps deps);

  void begin();
  void update();
  BootMode mode() const;

 private:
  void enterConfigMode();
  void enterSystemMode();
  void updateSetupShortcut(unsigned long now);
  void updateHomeSleep(unsigned long now);
  HomeSleepRequest makeHomeSleepRequest() const;

  BootControllerDeps deps_;
  BootMode mode_ = BootMode::System;
  bool started_ = false;
  unsigned long setupButtonsPressedSinceMs_ = 0;
  bool setupButtonsWerePressed_ = false;
  bool setupShortcutConsumed_ = false;
  unsigned long systemModeStartedAtMs_ = 0;
  bool homeSleepRequested_ = false;
};

}  // namespace homedeck
