#include "WifiRunner.h"

#include <string.h>
#include <esp_wifi.h> 
#include "../toolkit/DeviceInfo.h"
#include "../logging/NullLogger.h"


WifiRunner * wfr_self;

void wfr_WiFiEvent( WiFiEvent_t event, WiFiEventInfo_t info ) {
    wfr_self->dumpEventInfo( event, info );
}



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

    wfr_self = this;
    WiFi.onEvent(wfr_WiFiEvent);
}

void WifiRunner::dumpEventInfo( WiFiEvent_t event, WiFiEventInfo_t info )
{
    // this->logger->log("WiFi event: %d", event );
    // https://github.com/espressif/arduino-esp32/blob/a5c98a27f8cf0fa83e37fcd978db796e025089d9/libraries/Network/src/NetworkEvents.h#L129

    char mac[20];

    switch (event) {
        case ARDUINO_EVENT_WIFI_AP_START:           
            // this->logger->log("WiFi AP: started"); 
            this->apNumClients = 0;
            break;
        case ARDUINO_EVENT_WIFI_AP_STOP:            
            // this->logger->log("WiFi AP: stopped"); 
            this->apNumClients = 0;
            break;
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:    
            // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html#_CPPv428wifi_event_ap_staconnected_t
            // info.wifi_ap_staconnected
            this->apNumClients = WiFi.softAPgetStationNum();
            sprintf( mac, "%02x%02x%02x%02x%02x%02x",  
                info.wifi_ap_staconnected.mac[0],
                info.wifi_ap_staconnected.mac[1],
                info.wifi_ap_staconnected.mac[2],
                info.wifi_ap_staconnected.mac[3],
                info.wifi_ap_staconnected.mac[4],
                info.wifi_ap_staconnected.mac[5] );
            this->logger->log("~ WiFi AP: Client connected MAC '%s', clients=%d",
                mac,
                this->apNumClients
            ); 
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: 
            // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html#_CPPv431wifi_event_ap_stadisconnected_t
            // info.wifi_ap_stadisconnected
            this->apNumClients = WiFi.softAPgetStationNum();
            sprintf( mac, "%02x%02x%02x%02x%02x%02x",  
                info.wifi_ap_stadisconnected.mac[0],
                info.wifi_ap_stadisconnected.mac[1],
                info.wifi_ap_stadisconnected.mac[2],
                info.wifi_ap_stadisconnected.mac[3],
                info.wifi_ap_stadisconnected.mac[4],
                info.wifi_ap_stadisconnected.mac[5] );
            this->logger->log("~ WiFi AP: Client disconnected MAC '%s', reason %d, clients=%d",
                mac, 
                info.wifi_ap_stadisconnected.reason,
                this->apNumClients
            ); 
            WifiStatus_ClientDisconnected( mac );
            break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:   
            // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_netif_programming.html#_CPPv427ip_event_ap_staipassigned_t
            this->apNumClients = WiFi.softAPgetStationNum();
            this->anyClientConnected = true;
            char buffer[40];
            esp_ip4addr_ntoa( (const esp_ip4_addr_t*)&(info.wifi_ap_staipassigned.ip), buffer, 39);
            // this->logger->print( info.wifi_ap_staipassigned.ip );
            sprintf( mac, "%02x%02x%02x%02x%02x%02x",  
                info.wifi_ap_staipassigned.mac[0],
                info.wifi_ap_staipassigned.mac[1],
                info.wifi_ap_staipassigned.mac[2],
                info.wifi_ap_staipassigned.mac[3],
                info.wifi_ap_staipassigned.mac[4],
                info.wifi_ap_staipassigned.mac[5] );

            this->logger->log("~ WiFi AP: Client IP address %s - MAC '%s', clients=%d", 
                buffer,
                mac,
                this->apNumClients
            ); 
            WifiStatus_ClientConnected( mac, buffer );
            break;
        /*
        case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:  Serial.println("WiFi AP: Received probe request"); break;
        case ARDUINO_EVENT_WIFI_AP_GOT_IP6:         Serial.println("WiFi AP: AP IPv6 is preferred"); break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6:        Serial.println("WiFi AP: STA IPv6 is preferred"); break;
        */
        default:                                    break;
    }    
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
 * ESP32C3 SuperMini má dementní anténu; zavolejte fixPowerForEsp32C3SuperMini() pro snížení výkonu.
 * fix for https://github.com/espressif/arduino-esp32/issues/6767
 */
void WifiRunner::fixPowerForEsp32C3SuperMini()
{
    this->doFixPowerForEsp32C3Micro = true;
}

/** Přidá k určenému jménu AP ještě chip ID */
void WifiRunner::addChipIdToApHostname( bool addId )
{
    this->doAddChipIdToApHostname = addId;
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
    this->logger->log( "~ Startuji AP '%s'", ssid );
    WiFi.softAP(ssid, apPassword, SOFT_AP_CHANNEL, false );
    delay(100);

    this->startTime = millis();
    if( this->clientSsid[0]!=0 ) {
        this->logger->log( "~ Pripojuji se k '%s', ch %d", this->clientSsid, this->clientChannel );
        WiFi.persistent(false);
        WiFi.setAutoReconnect(true);
        WiFi.begin(this->clientSsid, this->clientPassword, this->clientChannel ); 
    } else {
        this->logger->log( "~ Chyba: nejsou nastaveny parametry klienta." );
    }

    if( this->doFixPowerForEsp32C3Micro ) {
        this->logger->log( "~  Snizeni vysilaciho vykonu o 8.5 dBm (ESP32-C3 SuperMini)" );
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
    this->logger->log("~ AP IP address: %s", this->apIp );

}

/** Pokud se spustí jen AP, je možné ho pak pomocí stopAp() a startApAgain() zastavit a znovu spustit pro řízení spotřeby.   */
void WifiRunner::startAp( const char * apSsid, const char * apPassword, IPAddress local_ip, IPAddress gateway, IPAddress subnet )
{
    this->mode = WIFIHELPER_AP;
    this->apRunning = true;

    this->ipLocal = local_ip;
    this->ipGw = gateway;
    this->ipSubnet = subnet;
    
    strcpy( this->apFullName, apSsid );
    if( this->doAddChipIdToApHostname ) {
        formatDeviceId( this->apFullName + strlen(this->apFullName) );
    }
    strcpy( this->apPwd, apPassword );
    
    WiFi.softAPConfig(this->ipLocal, this->ipGw, this->ipSubnet);
    // musi tady byt; kdyz se AP spusti _hned_, obcas to padne
    delay(100);

    this->logger->log( "~ Startuji AP '%s'", this->apFullName );
    WiFi.softAP(this->apFullName, this->apPwd, SOFT_AP_CHANNEL, false );

    if( this->doFixPowerForEsp32C3Micro ) {
        this->logger->log( "~ Snizeni vysilaciho vykonu o 8.5 dBm (ESP32-C3 SuperMini)" );
        // fix for https://github.com/espressif/arduino-esp32/issues/6767
        WiFi.setTxPower( ESP32C3MICRO_WIFI_POWER );
    }
    // musi tady byt delay; kdyz se operace spusti _hned_, obcas to padne
    delay(500);
    WiFi.softAPsetHostname(this->apFullName);
    
    IPAddress myIP = WiFi.softAPIP();
    logger->print( myIP );
    strcpy( this->apIp, logger->printed );
    logger->printed[0] = 0;
    this->logger->log("~ AP IP address: %s", this->apIp );
}

void WifiRunner::stopAp() {
    if( !this->apRunning || this->mode!=WIFIHELPER_AP ) return;
    this->apRunning = false;

    this->logger->log( "~ Vypinam AP '%s'", this->apFullName );
    WiFi.softAPdisconnect( true );
}

void WifiRunner::startApAgain() {
    if( this->apRunning || this->mode!=WIFIHELPER_AP ) return;
    this->apRunning = true;

    this->logger->log( "~ Zapinam AP '%s'", this->apFullName );
    
    WiFi.softAPConfig(this->ipLocal, this->ipGw, this->ipSubnet);
    // musi tady byt; kdyz se AP spusti _hned_, obcas to padne
    delay(100);

    WiFi.softAP(this->apFullName, this->apPwd, SOFT_AP_CHANNEL, false );

    if( this->doFixPowerForEsp32C3Micro ) {
        // fix for https://github.com/espressif/arduino-esp32/issues/6767
        WiFi.setTxPower( ESP32C3MICRO_WIFI_POWER );
    }

    // musi tady byt delay; kdyz se operace spusti _hned_, obcas to padne
    delay(500);
    WiFi.softAPsetHostname(this->apFullName);

    
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
        this->logger->log( "~ Pripojuji se k '%s', ch %d", this->clientSsid, this->clientChannel );
        WiFi.persistent(false);
        WiFi.setAutoReconnect(true);
        WiFi.begin(this->clientSsid, this->clientPassword, this->clientChannel ); 
    } else {
        this->logger->log( "~ Chyba: nejsou nastaveny parametry klienta." );
    }
    if( this->doFixPowerForEsp32C3Micro ) {
        Serial.println( "~  Snizeni vysilaciho vykonu o 8.5 dBm (ESP32-C3 SuperMini)" );
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
                    this->logger->print( ip );
                    strncpy( this->clientIp,logger->printed, WIFIHELPER_MAX_IP_LENGTH );
                    this->clientIp[WIFIHELPER_MAX_IP_LENGTH-1] = 0;
                    this->logger->log( "~ Wifi: CONNECTED, IP: %s, ch %d, %d dB, %d msec", 
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
                    this->logger->log( "~ Wifi: %s (%d)", getWifiStatusText(this->clientStatus), this->clientStatus );
                    break;
                
                default:
                    this->clientConnected = false;
                    this->logger->log( "~ Wifi: UNKNOWN status %d", this->clientStatus );
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
            this->logger->log( "~ Wifi: reconnect '%s' po %d sec od posledniho pokusu", 
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
