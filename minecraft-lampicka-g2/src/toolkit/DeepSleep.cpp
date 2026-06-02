
#include <Arduino.h>
#include "DeepSleep.h"

// https://docs.espressif.com/projects/arduino-esp32/en/latest/api/deepsleep.html
// (alternativne: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/reset_reason.html )


void getWakeupReason( char * target ) {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:     strcpy( target, "EXT0 - by RTC_IO" ); break;
        case ESP_SLEEP_WAKEUP_EXT1:     strcpy( target, "EXT1 - by RTC_CNTL" ); break;
        case ESP_SLEEP_WAKEUP_TIMER:    strcpy( target, "timer" ); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD: strcpy( target, "touchpad" ); break;
        case ESP_SLEEP_WAKEUP_ULP:      strcpy( target, "ULP program" ); break;
        default:                        sprintf( target, "not from deep sleep (%d)", wakeup_reason); break;
    }
}


/* Conversion factor for micro seconds to seconds */
#define uS_TO_S_FACTOR 1000000ULL 

void setDeepSleepTimer( long seconds ) {
    esp_sleep_enable_timer_wakeup(seconds * uS_TO_S_FACTOR);
}

void startDeepSleep( bool stopSerial ) {
    if( stopSerial ) {
        Serial.flush();
        Serial.end();
    }
    esp_deep_sleep_start();
}