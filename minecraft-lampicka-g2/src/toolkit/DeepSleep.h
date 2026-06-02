#ifndef ___DEEP_SLEEP_HELPER_H___
#define ___DEEP_SLEEP_HELPER_H___

#define DEEPSLEEP_VERSION "1.0 2026-06-02"

void getWakeupReason( char * target );
void setDeepSleepTimer( long seconds );
void startDeepSleep( bool stopSerial = true );

#endif