
#include "EasyWebServer.h"
#include "../logging/NullLogger.h"


EasyWebServer::EasyWebServer( int webPort, LoggerInterface * logger ) 
{
    if( logger == NULL ) {
        this->logger = (LoggerInterface*) new NullLogger();
    } else {
        this->logger = logger;
    }
    
    this->server = new AsyncWebServer(webPort);
    this->mode = EWS_NONE;
}


/**
 * Ziska IP adresu AP.
 * Target musi mit delku minimalne EWS_MAX_IP_LENGTH
 */
void EasyWebServer::getApIp( char * target ) 
{
    IPAddress myIP = WiFi.softAPIP();
    logger->print( myIP );
    strncpy( target, logger->printed, EWS_MAX_IP_LENGTH-1 );
    logger->printed[0] = 0;
    target[EWS_MAX_IP_LENGTH-1] = 0;
}

/**
 * Z relativni cesty slozi celou adresu pro redirect
 */
void EasyWebServer::setCaptivePortalUrl( const char * captivePortalPath ) {
    strcpy( this->captivePortalUrl, "http://" );
    this->getApIp( this->captivePortalUrl + 7 );
    strcat( this->captivePortalUrl, captivePortalPath );
}

void EasyWebServer::startWebserverOnApAndClient(  const char * captivePortalPath  )
{
    this->mode = EWS_AP_CLIENT;
    this->setCaptivePortalUrl( captivePortalPath );
    
    this->logger->log("EasyWebServer start (AP+STA mode)");
    this->begin();

    this->dnsServer.start(53, "*", WiFi.softAPIP()); 
}

void EasyWebServer::startWebserverOnAp( const char * captivePortalPath  )
{
    this->mode = EWS_AP;
    this->setCaptivePortalUrl( captivePortalPath );
    
    this->logger->log("EasyWebServer start (AP mode)");
    this->begin();

    this->dnsServer.start(53, "*", WiFi.softAPIP());
}


void EasyWebServer::startWebserverOnClient()
{
    this->mode = EWS_CLIENT;
    this->captivePortalUrl[0] = 0;

    Serial.println("EasyWebServer start (client mode)");
    this->begin();
}


/**
 * Musi byt volano z loop() !
 */
void EasyWebServer::process()
{
    if( this->mode==EWS_AP || this->mode==EWS_AP_CLIENT) {
        this->dnsServer.processNextRequest();
    }
}

const char *EasyWebServer::getQueryParamAsString(AsyncWebServerRequest *request, const char *paramName, const char *defaultValue)
{
    if(request->hasParam(paramName) ) { 
        return request->getParam(paramName)->value().c_str();
    } else {
        return defaultValue;
    }
}

long EasyWebServer::getQueryParamAsLong(AsyncWebServerRequest *request, const char *paramName, long defaultValue)
{
    if(request->hasParam(paramName) ) { 
        return atol( request->getParam(paramName)->value().c_str() );
    } else {
        return defaultValue;
    }
}


/**
 * Captive portal; nedela nic jineho nez ze presmerovava na urcene URL
*/
class CaptiveRequestHandler : public AsyncWebHandler {
    public:
        CaptiveRequestHandler( LoggerInterface * logger, EwsMode mode, char * captivePortalUrl, char * myIp  ) { 
            this->logger = logger;
            this->mode = mode;
            this->captivePortalUrl = captivePortalUrl;
            strcpy( this->myIp, myIp );
        }
        virtual ~CaptiveRequestHandler() {}
        
        bool canHandle(__unused AsyncWebServerRequest *request) const override {
            // da se kontrolovat treba request->addInterestingHeader("ANY");

            // neznamy host = filtrujeme
            if( request->host()==NULL ) {
                return true;
            }
            
            // pokud prisel pozadavek na IP adresu klienta, odbavit (nefiltrovat)
            if( this->mode!=EWS_AP && ON_STA_FILTER(request)) {
                //D/ this->logger->log( "  > STA: %s%s", request->host().c_str(), request->url().c_str() );
                return false;    
            }

            // pozadavek prisel na AP; filtrujeme vsechny pozadavky na servery, co nejsme my (nejsou na nasi IP adresu)
            if( strcmp(request->host().c_str(), this->myIp)!=0 ) {
                //D/ this->logger->log( "  > AP jina IP: %s%s", request->host().c_str(), request->url().c_str() );
                return true;
            }
            // pokud je pozadavek na nase jmeno, timto filtrem ho neresim, aby fungovalo 404
            //D/ this->logger->log( "  > AP nase IP: %s%s", request->host().c_str(), request->url().c_str() );
            return false;
        }

        void handleRequest(AsyncWebServerRequest *request) {
            //D/ this->logger->log( "  > %s%s", request->host().c_str(), request->url().c_str() );
            request->redirect( this->captivePortalUrl );
        }

    private:
        LoggerInterface * logger;
        EwsMode mode;
        char * captivePortalUrl;
        char myIp[EWS_MAX_IP_LENGTH];

};



/**
 * Konfigurace webserveru; konfiguraci podle aplikace nastavte v userRoutes() !
 */
void EasyWebServer::begin() 
{
    // zavolame fci pro vytvoreni uzivatelskych rout - callback do INO aplikace
    userRoutes( this->server );

    // pokud jsme jen client, captive portal nedava smysl; pro AP a AP+client ho chceme
    if( this->mode!=EWS_CLIENT ) {
        char myIp[EWS_MAX_IP_LENGTH] ;
        this->getApIp( myIp );
        this->server->addHandler( new CaptiveRequestHandler(this->logger, this->mode, this->captivePortalUrl, myIp ) );
    }

    this->server->begin();
}





