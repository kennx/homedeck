#pragma once

namespace homedeck {

struct EnvironmentReading {
  bool ok = false;
  float temperatureCelsius = 0.0f;
  float humidityPercent = 0.0f;
};

EnvironmentReading readSht40Environment();

}  // namespace homedeck
