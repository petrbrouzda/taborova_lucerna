#ifndef __EWEB_SERVER___H_
#define __EWEB_SERVER___H_

/**
Zjednodušení kombinace webserver + DNS server pro zařízení, která vystavují konfigurační interface přes web.

Pouzivaji se tyto dve knihovny:
- https://github.com/ESP32Async/ESPAsyncWebServer
- https://github.com/ESP32Async/AsyncTCP 
*/

#include <Arduino.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#include "../logging/LoggerInterface.h"

#define EASYWEBSERVER_VERSION "2.2 2026-01-20"

// takovahle funkce musi byt v INO kodu aplikace - user code v ni definuje platne cesty ve webserveru
void userRoutes( AsyncWebServer * server );


enum EwsMode {
    EWS_NONE,
    EWS_CLIENT,
    EWS_AP,
    EWS_AP_CLIENT,
};


#define EWS_MAX_IP_LENGTH 50
#define MAX_CAPTIVE_URL_LEN 100

class EasyWebServer
{
    public:
        EasyWebServer( int webPort, LoggerInterface * logger = NULL );

        /** pouzijte, pokud ma webserver fungovat jen na AP rozhrani - dela i self-catch DNS a captive portal  */
        void startWebserverOnAp( const char * captivePortalPath );
        /** pouzijte, pokud ma webserver fungovat na AP i client rozhrani - dela i self-catch DNS a captive portal */
        void startWebserverOnApAndClient( const char * captivePortalPath );
        /** pouzijte, pokud ma webserver fungovat jen na client rozhrani  */
        void startWebserverOnClient();

        /** 
         * Odbavuje DNS a WiFi status pozadavky, musi byt volano z loop().
         * Na ESP32 core 3.x už nejspíš není potřeba (podle komentářů ve zdrojácích core)
         */
        void process();

        const char * getQueryParamAsString( AsyncWebServerRequest *request, const char * paramName, const char * defaultValue );
        long getQueryParamAsLong( AsyncWebServerRequest *request, const char * paramName, long defaultValue );

        const char* HTML_UTF8 = "text/html; charset=utf-8";

    private:
        LoggerInterface * logger;
        AsyncWebServer * server;
        DNSServer dnsServer;

        EwsMode mode;

        char captivePortalUrl[MAX_CAPTIVE_URL_LEN];

        /** start webserveru */
        void begin();

        void getApIp( char * target );
        void setCaptivePortalUrl( const char * captivePortalPath );
};


#endif
