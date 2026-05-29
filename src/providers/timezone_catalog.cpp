#include "providers/timezone_catalog.h"

#include <iterator>

namespace homedeck {
namespace {

constexpr TimezoneInfo kTimezones[] = {
    {"Asia/Shanghai", "CST-8", "Asia/Shanghai"},
    {"UTC", "UTC0", "UTC"},
};

}  // namespace

const TimezoneInfo* findTimezoneByIana(std::string_view iana) {
  for (const auto& timezone : kTimezones) {
    if (iana == timezone.iana) {
      return &timezone;
    }
  }
  return nullptr;
}

const TimezoneInfo* defaultTimezone() {
  return &kTimezones[0];
}

const TimezoneInfo* timezoneCatalog(std::size_t* count) {
  if (count != nullptr) {
    *count = std::size(kTimezones);
  }
  return kTimezones;
}

}  // namespace homedeck
