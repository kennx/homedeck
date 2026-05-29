#pragma once

#include <string>

namespace homedeck {

bool connectWifiPreservingAccessPoint(
    const std::string& ssid,
    const std::string& password,
    unsigned long timeoutMs = 10000);

}  // namespace homedeck
