#pragma once

#include <ctime>
#include <string>

struct TimeSnapshot {
  std::string timeText;
  std::string dateText;
  bool timeValid = false;
  bool timeSynced = false;
};

class TimeService {
 public:
  bool begin(const char* timezonePosix, const char* ntpServer);
  TimeSnapshot snapshot() const;
  bool syncFromNtp();
  // 恢复持久化的最近成功同步时间；仅在当前系统时间已有效时恢复 synced 状态。
  void restoreSyncState(time_t lastSuccessfulSyncUnix);
  time_t lastSuccessfulSyncUnix() const;

 private:
  std::string timezonePosix_;
  std::string ntpServer_;
  bool rtcAvailable_ = false;
  bool timeSynced_ = false;
  time_t lastSyncedAt_ = 0;
};
