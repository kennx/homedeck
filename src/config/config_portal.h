#pragma once

#include <DNSServer.h>
#include <WebServer.h>

#include <functional>
#include <string>
#include <vector>

#include "config/config_types.h"

namespace homedeck {

using ConfigPortalSaveCallback = std::function<ConfigValidationResult(
    const SetupConfig& config,
    const ManualDateTime& manualDateTime)>;

class ConfigPortal {
 public:
  void begin(
      const std::string& apSsid,
      const SetupConfig& defaults,
      ConfigPortalSaveCallback onSave);
  void handleClient();

 private:
  std::vector<WifiNetwork> scanWifiNetworks();
  SetupConfig readConfigFromRequest();
  ManualDateTime readManualTimeFromRequest();
  void handleIndex();
  void handleSave();
  void redirectToIndex();
  void registerCaptivePortalRoutes();
  void sendPage(int status, const SetupConfig& values, const std::string& message);

  WebServer server_{80};
  DNSServer dnsServer_;
  std::string apSsid_;
  SetupConfig defaults_{};
  std::vector<WifiNetwork> networks_;
  ConfigPortalSaveCallback onSave_;
  bool restartScheduled_ = false;
  unsigned long restartAtMs_ = 0;
};

}  // namespace homedeck
