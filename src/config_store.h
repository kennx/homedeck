#pragma once

#include <Preferences.h>

#include "boot_controller.h"
#include "config_types.h"

namespace homedeck {

class ConfigStore {
 public:
  bool begin();
  SetupConfig loadSetupConfig() const;
  bool saveSetupConfig(const SetupConfig& config);
  BootFlags loadBootFlags() const;
  bool saveConfigured(bool configured);
  bool setForceConfigOnNextBoot(bool enabled);
  bool clearForceConfigOnNextBoot();

 private:
  mutable Preferences prefs_;
  bool started_ = false;
};

}  // namespace homedeck
