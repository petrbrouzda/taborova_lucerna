#ifndef __WIFIHELPER_H
#define __WIFIHELPER_H

#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>

#include "../logging/LoggerInterface.h"

#define WIFIRUNNER_VERSION "3.1 2026-06-02"

/**
 * Nástroj pro práci s Wifi. Jednoduše konfiguruje wifi jak STA (client), tak AP.
 * Zajišťuje callbacky do user kódu pro připojení na STA rozhraní a pro ukončení tohoto připojení.
 * Automaticky dělá reconect na STA rozhraní.
  */

enum WfhMode {
    WIFIHELPER_NONE,
    WIFIHELPER_CLIENT,
    WIFIHELPER_AP,
    WIFIHELPER_AP_CLIENT,
};

#define WIFIHELPER_CONFIG_MAX_LEN 50
#define WIFIHELPER_MAX_IP_LENGTH 34

/* 
Callbacky pro stav Wifi klienta.
Pokud se používá jen AP režim (startAp), tak se nikdy nepoužijí; ale musí být definované.
*/
void WifiStatus_Connected( const char * ssid, int rssi, int channel );
void WifiStatus_NotConnected( int status );

/*
Callbacky pro stav Wifi AP 
Pokud se používá jen režim STA (klient), tak se nikdy nepoužijí; ale musí být definované.
*/
void WifiStatus_ClientConnected( char * mac, char * ip );
void WifiStatus_ClientDisconnected( char * mac );


/** 
 * ESP32C3 SuperMini má dementní anténu; zavolejte fixPowerForEsp32C3Micro() pro snížení výkonu.
 * fix for https://github.com/espressif/arduino-esp32/issues/6767
 */
#define ESP32C3MICRO_WIFI_POWER WIFI_POWER_8_5dBm


/** Pokud neni spojeni po tuto dobu, udela se reset wifi klienta */
#define RESTART_WIFI_AFTER_MSEC 120000

/** Na jakém kanálu má být AP? */
#define SOFT_AP_CHANNEL 8

class WifiRunner
{
    public:
        /** Lépe použít asynchronní logger, protože se do něj posílají i wifi eventy. */
        WifiRunner( LoggerInterface * logger = NULL );
        void setClientConfig( const char * ssid, const char * password, int channel=0 );
        void setClientHostname( const char * hostname, bool addChipId );

        /** 
         * ESP32C3 Micro má dementní anténu; zavolejte tohle pro snížení výkonu.
         * fix for https://github.com/espressif/arduino-esp32/issues/6767
         * 
         * Volat před start*()
         */
        void fixPowerForEsp32C3SuperMini();

        /** 
         * Přidá k určenému jménu AP ještě chip ID
         * Volat před startApAndClient() a startAP()
         */
        void addChipIdToApHostname( bool addId = true );

        /** pred startApAndClient je nutne zavolat setClientConfig()! */
        void startApAndClient( const char * apSsid, const char * apPassword, IPAddress local_ip, IPAddress gateway, IPAddress subnet );

        /** Pokud se spustí jen AP, je možné ho pak pomocí stopAp() a startApAgain() zastavit a znovu spustit pro řízení spotřeby.   */
        void startAp( const char * apSsid, const char * apPassword, IPAddress local_ip, IPAddress gateway, IPAddress subnet );

        /** Pokud se provozuje jen AP, je možné ho pomocí stopAp() zastavit, ušetří se 60 mA spotřeby. Kromě AP vypne i celou WiFi! */
        void stopAp();

        /** Pokud bylo AP zastaveno pomoc stopAp(), tohle ho opět spustí. Pokud používáte webserver, není třeba webserver pouštět znovu, funguje dál. */
        void startApAgain();

        /** spustí STA (client) režim. Pred startClient je nutne zavolat setClientConfig() */
        void startClient();

        /** vynuti restart STA (client) spojeni */
        void reconnectClient();

        /** Vyhodnocuje stav Wifi STA (clienta) a volá callbacky.
         */
        void process();

        /** true = STA (client) je připojeno k síti */
        bool isClientConnected();
        /** IP adresa na STA rozhraní, jako text */
        const char * getClientIp();
        /** odkaz na SSID sítě, do které se klient chce připojit */
        const char * getClientSsid();

        /** IP adresa na AP rozhraní, jako text */
        const char * getApIp();

        /** millis time, kdy se naposledy zařízení připojilo k wifi (začátek spojení)*/
        long lastConnectedTime = 0;
        /** millis time, kdy se naposledy zařízení odpojilo od wifi (konec spojení) */
        long lastDisconnectedTime = 0;

        void dumpEventInfo( WiFiEvent_t event, WiFiEventInfo_t info );

        /** počet aktuálně připojených klientů k AP */
        int apNumClients = 0;

        /** byl za celou dobu nějaký připojený klient k AP? */
        bool anyClientConnected = false;

    private:
        LoggerInterface * logger; 
        WfhMode mode = WIFIHELPER_NONE;

        bool doFixPowerForEsp32C3Micro = false;
        bool doAddChipIdToApHostname = false;

        char clientSsid[WIFIHELPER_CONFIG_MAX_LEN];
        char clientPassword[WIFIHELPER_CONFIG_MAX_LEN];
        int clientChannel;

        char clientHostname[WIFIHELPER_CONFIG_MAX_LEN];

        char apSsid[WIFIHELPER_CONFIG_MAX_LEN];
        
        int clientStatus = -1;
        bool clientConnected = false;
        char clientIp[WIFIHELPER_MAX_IP_LENGTH];

        char apIp[WIFIHELPER_MAX_IP_LENGTH];

        long startTime = 0;

        // --- konfigurace AP, pro funkci startApAgain
        char apPwd[40];
        char apFullName[50];
        IPAddress ipLocal;
        IPAddress ipGw;
        IPAddress ipSubnet;
        bool apRunning;

};

#endif