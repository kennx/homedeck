#pragma once

#include <sys/time.h>

using sntp_sync_time_cb_t = void (*)(struct timeval*);

enum sntp_sync_status_t {
  SNTP_SYNC_STATUS_RESET = 0,
  SNTP_SYNC_STATUS_COMPLETED = 1,
  SNTP_SYNC_STATUS_IN_PROGRESS = 2,
};

inline sntp_sync_time_cb_t gFakeSntpCallback = nullptr;
inline sntp_sync_status_t gFakeSntpSyncStatus = SNTP_SYNC_STATUS_RESET;

inline void fakeSntpReset() {
  gFakeSntpCallback = nullptr;
  gFakeSntpSyncStatus = SNTP_SYNC_STATUS_RESET;
}

inline void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t callback) {
  gFakeSntpCallback = callback;
}

inline sntp_sync_status_t sntp_get_sync_status() {
  return gFakeSntpSyncStatus;
}

inline void sntp_set_sync_status(sntp_sync_status_t status) {
  gFakeSntpSyncStatus = status;
}

inline void fakeSntpNotifySync() {
  if (gFakeSntpCallback == nullptr) {
    return;
  }

  timeval value{};
  gFakeSntpCallback(&value);
  gFakeSntpSyncStatus = SNTP_SYNC_STATUS_COMPLETED;
}
