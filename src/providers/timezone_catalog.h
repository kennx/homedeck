#pragma once

#include <cstddef>
#include <string_view>

namespace homedeck {

struct TimezoneInfo {
  const char* iana;
  const char* posix;
  const char* label;
};

const TimezoneInfo* findTimezoneByIana(std::string_view iana);
const TimezoneInfo* defaultTimezone();
const TimezoneInfo* timezoneCatalog(std::size_t* count);

}  // namespace homedeck
