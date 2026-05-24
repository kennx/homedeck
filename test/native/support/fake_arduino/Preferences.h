#pragma once

#include <map>
#include <string>

inline std::map<std::string, std::string> gFakePreferenceStrings;
inline std::map<std::string, bool> gFakePreferenceBools;
inline bool gFakePreferencesBeginResult = true;

inline void fakePreferencesReset() {
  gFakePreferenceStrings.clear();
  gFakePreferenceBools.clear();
  gFakePreferencesBeginResult = true;
}

class Preferences {
 public:
  bool begin(const char*, bool = false) {
    started_ = gFakePreferencesBeginResult;
    return started_;
  }

  void end() {
    started_ = false;
  }

  std::string getString(const char* key, const char* defaultValue = "") const {
    const auto found = gFakePreferenceStrings.find(key);
    return found == gFakePreferenceStrings.end() ? std::string(defaultValue) : found->second;
  }

  std::size_t putString(const char* key, const char* value) {
    if (!started_) {
      return 0;
    }
    gFakePreferenceStrings[key] = value != nullptr ? value : "";
    return gFakePreferenceStrings[key].size();
  }

  bool getBool(const char* key, bool defaultValue = false) const {
    const auto found = gFakePreferenceBools.find(key);
    return found == gFakePreferenceBools.end() ? defaultValue : found->second;
  }

  bool putBool(const char* key, bool value) {
    if (!started_) {
      return false;
    }
    gFakePreferenceBools[key] = value;
    return true;
  }

 private:
  bool started_ = false;
};
