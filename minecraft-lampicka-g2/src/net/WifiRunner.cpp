#include "WifiRunner.h"

#include <string.h>
#include <esp_wifi.h> 
#include "../toolkit/DeviceInfo.h"
#include "../logging/NullLogger.h"


WifiRunner::WifiRunner(LoggerInterface *logger)
{
    if( logger == NULL ) {
        this->logger = (LoggerInterface*) new NullLogger();
    } else {
        this->logger = logger;
    }

    this->clientIp[0] = 0;
    this->clientSsid[0] = 0;
    this->apIp[0] = 0;
    this->clientHostname[0] = 0;
    this->startTime = millis();
}

void WifiRunner::setClientConfig(const char *ssid, const char *password, int channel )
{
    strncpy( this->clientSsid, ssid, WIFIHELPER_CONFIG_MAX_LEN-1 );
    this->clientSsid[WIFIHELPER_CONFIG_MAX_LEN-1]  = 0;
    strncpy( this->clientPassword, password, WIFIHELPER_CONFIG_MAX_LEN-1 );
    this->clientPassword[WIFIHELPER_CONFIG_MAX_LEN-1]  = 0;
    this->clientChannel = channel;
}

void WifiRunner::setClientHostname(const char *hostname, bool addChipId )
{
    strncpy( this->clientHostname, hostname, WIFIHELPER_CONFIG_MAX_LEN-1 );
    this->clientHostname[WIFIHELPER_CONFIG_MAX_LEN-1]  = 0;
    formatDeviceId( this->clientHostname + strlen(this->clientHostname) );
}

/** 
 * ESP32C3 Micro má dementní anténu; zavolejte fixPowerForEsp32C3Micro() pro snížení výkonu.
 * fix for https://github.com/espressif/arduino-esp32/issues/6767
 */
void WifiRunner::fixPowerForEsp32C3Micro()
{
    this->doFixPowerForEsp32C3Micro = true;
}

/** Přidá k určenému jménu AP ještě chip ID */
void WifiRunner::addChipIdToApHostname()
{
    this->doAddChipIdToApHostname = true;
}


/*
  esp_wifi_set_mode(WIFI_MODE_STA);

  // Create a WiFiConfig struct
  wifi_config_t wifi_config;

  // Set the channel
  wifi_config.sta.channel = 6; // Set to your desired channel

  // Set SSID and password
  memcpy(wifi_config.sta.ssid, "YOUR_SSID", strlen("YOUR_SSID"));
  memcpy(wifi_config.sta.password, "YOUR_PASSWORD", strlen("YOUR_PASSWORD"));

  // Apply the configuration
  esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);

  // Connect to the network
  esp_wifi_connect();
*/


void WifiRunner::startApAndClient(const char * apSsid, const char * apPassword, IPAddress local_ip, IPAddress gateway, IPAddress subnet)
{
    this->mode = WIFIHELPER_AP_CLIENT;

    if( this->clientHostname[0] != 0 ) {
        WiFi.setHostname( this->clientHostname );
        delay(50);
    }
    
    WiFi.softAPConfig(local_ip, gateway, subnet);
    // musi tady byt; kdyz se AP spusti _hned_, obcas to padne
    delay(100);

    char ssid[100];
    strcpy( ssid, apSsid );
    if( this->doAddChipIdToApHostname ) {
        formatDeviceId( ssid + strlen(ssid) );
    }
    this->logger->log( "Startuji AP '%s'", ssid );
    WiFi.softAP(ssid, apPassword, SOFT_AP_CHANNEL, false );
    delay(100);

    this->startTime = millis();
    if( this->clientSsid[0]!=0 ) {
        this->logger->log( "Pripojuji se k '%s', ch %d", this->clientSsid, this->clientChannel );
        WiFi.persistent(false);
        WiFi.setAutoReconnect(true);
        WiFi.begin(this->clientSsid, this->clientPassword, this->clientChannel ); 
    } else {
        this->logger->log( "Chyba: nejsou nastaveny parametry klienta." );
    }

    if( this->doFixPowerForEsp32C3Micro ) {
        this->logger->log( "++ Snizeni vysilaciho vykonu o 8.5 dBm (ESP32-C3 Micro)" );
        // fix for https://github.com/espressif/arduino-esp32/issues/6767
        WiFi.setTxPower( ESP32C3MICRO_WIFI_POWER );
    }

    // musi tady byt; kdyz se operace spusti _hned_, obcas to padne
    delay(500);
    WiFi.softAPsetHostname(ssid);

    IPAddress myIP = WiFi.softAPIP();
    logger->print( myIP );
    strcpy( this->apIp, logger->printed );
    logger->printed[0] = 0;
    this->logger->log("AP IP address: %s", this->apIp );

}

void WifiRunner::startAp( const char * apSsid, const char * apPassword, IPAddress local_ip, IPAddress gateway, IPAddress subnet )
{
    this->mode = WIFIHELPER_AP;
    
    WiFi.softAPConfig(local_ip, gateway, subnet);
    // musi tady byt; kdyz se AP spusti _hned_, obcas to padne
    delay(100);

    char ssid[100];
    strcpy( ssid, apSsid );
    if( this->doAddChipIdToApHostname ) {
        formatDeviceId( ssid + strlen(ssid) );
    }
    this->logger->log( "Startuji AP '%s'", ssid );
    WiFi.softAP(ssid, apPassword, SOFT_AP_CHANNEL, false );

    if( this->doFixPowerForEsp32C3Micro ) {
        this->logger->log( "++ Snizeni vysilaciho vykonu o 8.5 dBm (ESP32-C3 Micro)" );
        // fix for https://github.com/espressif/arduino-esp32/issues/6767
        WiFi.setTxPower( ESP32C3MICRO_WIFI_POWER );
    }

    // musi tady byt; kdyz se operace spusti _hned_, obcas to padne
    delay(500);
    WiFi.softAPsetHostname(ssid);
    
    IPAddress myIP = WiFi.softAPIP();
    logger->print( myIP );
    strcpy( this->apIp, logger->printed );
    logger->printed[0] = 0;
    this->logger->log("AP IP address: %s", this->apIp );
}


void WifiRunner::startClient()
{
    this->mode = WIFIHELPER_CLIENT;

    if( this->clientHostname[0] != 0 ) {
        WiFi.setHostname( this->clientHostname );
        delay(50);
    }

    this->startTime = millis();
    if( this->clientSsid[0]!=0 ) {
        this->logger->log( "Pripojuji se k '%s', ch %d", this->clientSsid, this->clientChannel );
        WiFi.persistent(false);
        WiFi.setAutoReconnect(true);
        WiFi.begin(this->clientSsid, this->clientPassword, this->clientChannel ); 
    } else {
        this->logger->log( "Chyba: nejsou nastaveny parametry klienta." );
    }
    if( this->doFixPowerForEsp32C3Micro ) {
        Serial.println( "++ Snizeni vysilaciho vykonu o 8.5 dBm (ESP32-C3 Micro)" );
        // fix for https://github.com/espressif/arduino-esp32/issues/6767
        WiFi.setTxPower( ESP32C3MICRO_WIFI_POWER );
    }
}

void WifiRunner::reconnectClient()
{
    this->startTime = millis();
    WiFi.disconnect();
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(this->clientSsid, this->clientPassword, this->clientChannel ); 
}


void WifiRunner::process()
{
    if( this->mode==WIFIHELPER_AP_CLIENT || this->mode==WIFIHELPER_CLIENT) {
        IPAddress ip;

        int prevStatus = this->clientStatus;
        bool prevConnected = this->clientConnected;

        this->clientStatus = WiFi.status();
        if( prevStatus != this->clientStatus ) {
            switch( this->clientStatus ) {
                case WL_CONNECTED:
                    this->clientConnected = true;
                    this->lastConnectedTime = millis();
                    ip = WiFi.localIP();
                    logger->print( ip );
                    strncpy( this->clientIp,logger->printed, WIFIHELPER_MAX_IP_LENGTH );
                    this->clientIp[WIFIHELPER_MAX_IP_LENGTH-1] = 0;
                    this->logger->log( "Wifi: CONNECTED, IP: %s, ch %d, %d dB, %d msec", 
                            logger->printed, 
                            WiFi.channel(), WiFi.RSSI(),
                            millis() - this->startTime  );
                    WifiStatus_Connected( this->clientIp, WiFi.RSSI(), WiFi.channel() );
                    break;                

                case WL_IDLE_STATUS:
                case WL_NO_SSID_AVAIL:
                case WL_SCAN_COMPLETED:
                case WL_CONNECT_FAILED:
                case WL_CONNECTION_LOST:
                case WL_DISCONNECTED:
                    this->clientConnected = false;
                    this->logger->log( "Wifi: %s (%d)", getWifiStatusText(this->clientStatus), this->clientStatus );
                    break;
                
                default:
                    this->clientConnected = false;
                    this->logger->log( "Wifi: UNKNOWN status %d", this->clientStatus );
                    break;
            } // switch( this->clientStatus ) {                      

            if( prevConnected && !this->clientConnected ) {
                // byl spojeny a uz neni
                // vyresetujeme cas startu
                this->startTime = millis();
                this->lastDisconnectedTime = millis();
                WifiStatus_NotConnected( this->clientStatus );
            }

        } // if( prevStatus != this->clientStatus ) {

        if( (!this->clientConnected) && ((millis() - this->startTime) > RESTART_WIFI_AFTER_MSEC) )  {
            // restartujeme wifi klienta
            this->logger->log( "Wifi: reconnect '%s' po %d sec od posledniho pokusu", 
                        this->clientSsid, 
                        (millis() - this->startTime)/1000L
                     );
            this->reconnectClient();
        }

    } // if( this->mode==EWS_AP_CLIENT || this->mode==EWS_CLIENT) {
}

bool WifiRunner::isClientConnected()
{
    return this->clientConnected;
}

const char * WifiRunner::getClientIp()
{
    if( this->clientConnected ) {
        return this->clientIp;
    } else {
        return "";
    }
}

const char *WifiRunner::getClientSsid()
{
    return this->clientSsid;
}

const char * WifiRunner::getApIp()
{
    return this->apIp;
}
