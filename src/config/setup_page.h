#pragma once

#include <string>
#include <vector>

#include "config/config_types.h"

namespace homedeck {

std::vector<WifiNetwork> selectTopWifiNetworks(
    const std::vector<WifiNetwork>& networks,
    std::size_t limit);
std::string htmlEscape(const std::string& value);
std::string buildSetupPageHtml(
    const std::string& apSsid,
    const SetupConfig& values,
    const std::vector<WifiNetwork>& networks,
    const std::string& message);

}  // namespace homedeck
