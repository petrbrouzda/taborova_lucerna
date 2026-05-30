#ifndef DEVICE_INFO__H
#define DEVICE_INFO__H

/**
 * Nástroje pro výpis různých ESP-related informací
 */

#include "../logging/LoggerInterface.h"

#define DEVICEINFO_VERSION "2.1 2026-01-19"


#define WIFI_POWER_OFF -999

/**
 * Vypíše informaci o zařízení (typ čipu, rychlost, RAM)
 */
void formatDeviceInfo( char * out );

/**
 * Překládá stav Wifi na text
 */
const char * getWifiStatusText( int status );

/**
 * Vrací ESP chip ID
 */
void formatDeviceId( char * buf );


class MemoryHelper
{
    public:
        MemoryHelper( LoggerInterface * logger );

        /**
         * Vypíše stav/změnu heapu
         */
        void printFreeHeap();

        /**
         * Pro debugovani vyuziti pameti vlozte do kodu:
         *    memInfo( __FUNCTION__, __LINE__ );
         * A vypise se:
         *    ~ [startWifi:107] heap: 183440, delta -49904
         * tj. pozice ve zdrojaku, volny heap a zmena proti poslednimu volani memInfo
         */
        void memInfo( const char * func, int line);

    private:
        LoggerInterface * logger; 
        int prevFreeHeap = 0;
        int prevFreeHeap2 = 0;
};



#endif