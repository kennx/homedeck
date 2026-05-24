#pragma once

#include <string_view>

#include "config_types.h"

namespace homedeck {

bool parseManualDateTime(std::string_view value, ManualDateTime* out);
bool isManualDateTimeValid(const ManualDateTime& value);
ConfigValidationResult validateSetupSubmission(
    const SetupConfig& config,
    const ManualDateTime& manualDateTime);

}  // namespace homedeck
