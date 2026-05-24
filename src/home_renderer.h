#pragma once

#include <string>

namespace homedeck {

class HomeRenderer {
 public:
  void render();
  void renderConfigPortal(const std::string& apSsid, const std::string& ipAddress);
};

}  // namespace homedeck
