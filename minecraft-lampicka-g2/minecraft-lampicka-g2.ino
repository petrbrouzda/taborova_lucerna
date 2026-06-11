/*
FQBN: esp32:esp32:esp32c3:CDCOnBoot=cdc,CPUFreq=80,FlashFreq=40,FlashMode=dio
*/

/*
Piny:
  tlačítko (proti zemi) - 7
  odporový dělič na akumulátoru - 0
  WS2812B led - 10
*/
// Which pin on the Arduino is connected to the NeoPixels?
#define LED_PIN 10
#define TLACITKO 7
#define BATTERY_PIN 0



/* AsyncLogger se pouziva pro logovani udalosti v asynchronnich aktivitach (webserver...).
Uklada zaznamy do pole v pameti a ty se pak vypisou v loop() pomoci volani dumpTo(); */
#include "src/logging/AsyncLogger.h"
AsyncLogger asyncLogger;

/* SerialLogger vypisuje přímo na sériový port*/
#include "src/logging/SerialLogger.h"
SerialLogger serialLogger( &Serial );

/*
Pokud nechcete nelogovat vubec nic, pak všude, kde se má předávat LoggerInterface, 
předejte buď NULL nebo lépe pointer na instanci NullLogger.
*/


/** Sdileny stav aplikace - objekt drzici napr. chybu inicializace kamery, aby se dala vypsat uživateli ve webu */
#include "src/toolkit/AppState.h"
AppState appState( &serialLogger );


/*
Jednoducha obsluha wifi.
*/
#include "src/net/WifiRunner.h"
WifiRunner wifirunner( &serialLogger );


#include "src/toolkit/map_double.h"

/*
WebServer pro možnost administrace zařízení.
Přes objekt EasyWebServer se používají tyto dvě knihovny:
- https://github.com/ESP32Async/ESPAsyncWebServer
- https://github.com/ESP32Async/AsyncTCP 

V kódu volaném z webserveru je NUTNÉ používat asyncLogger, aby se akce v http callbacích nezdržovaly a nedocházelo ke kolizím na výstupu.
*/
#include "src/net/EasyWebServer.h"
#include "webserver-config.h"
EasyWebServer webserver( WEB_PORT, &asyncLogger );

bool vypinatWifi = true;


/*
Konfigurace.
Je nutny alespon kousek filesystemu SPIFFS.
*/
#include "src/toolkit/BasicConfig.h"
#include "src/toolkit/ConfigProviderSpiffs.h"
// nástroj pro držení konfigurace
BasicConfig config;
// načítání a ukládání konfigurace na SPIFFS; může dostat serialLogger, protože poběží synchronně v loopu
ConfigProviderSpiffs configProvider( &config, &appState, &serialLogger );

/*
Periodicke ulohy
https://github.com/joysfera/arduino-tasker
*/
#define TASKER_MAX_TASKS 32
#include <Tasker.h>
Tasker tasker;


// funkce formatDeviceInfo() a výpis stavu paměti
#include "src/toolkit/DeviceInfo.h"
MemoryHelper memoryHelper( &serialLogger );


#include "src/toolkit/DeepSleep.h"


#include <Adafruit_NeoPixel.h>
// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 11
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// tlacitko
int stavTlacitka;


// limity pro baterku
#define BAT_LIMIT_1 3.1
#define BAT_LIMIT_2 3.0

/**
napeti baterie
*/
float uBat = 0;


// vnitrni stavy aplikace
int brightness = 128;
bool brightnessChange = false;
int color = 0xffffff;
bool rozsviceno = false;
bool shutdownStarted = false;

/**
Režim nastavený z webserveru nebo tlačítkem:
1 = ohen
2 = morseovka
3 = konstantni barva
4 = blikani
*/
int aktualniRezim = 1;



// cervena color==16711680 zelena color==65280 modra color==255

/**
 * Při startu bliknutím ukáže stav baterky.
 */
void signalizujStavBaterky() {
  if( uBat > 3.85 ) {
    color = 65280;
  } else if( uBat > 3.5 ) {
    color = 255;
  } else {
    color = 16711680;
  }
  brightness = 64;
  rozsvit();
  delay(1000);
  zhasni();
  delay(500);
}


void setup() {
  Serial.begin(115200);

  serialLogger.log( "%s; %s %s", __FILE__, __DATE__, __TIME__  );
 
  // načtení konfigurace z SPIFFS
  configProvider.openFsAndLoadConfig();

  // jméno Wifi AP určené ve webserver-config.h bude rozšířeno o ID čipu, třeba "ESP_154154" 
  // wifirunner.addChipIdToApHostname();

  wifirunner.fixPowerForEsp32C3SuperMini();
  wifirunner.startAp( AP_SSID, AP_PASSWORD, local_ip, gateway, subnet );
  webserver.startWebserverOnApAndClient( CAPTIVE_PORTAL_REDIRECT_REL );

  pinMode( TLACITKO, INPUT_PULLUP );
  stavTlacitka = digitalRead( TLACITKO );

  // v ESP32 core 3.x prejmenovano, v dokumentaci je stale ADC_ATTEN_DB_11
  analogSetAttenuation(ADC_11db );
  pinMode( BATTERY_PIN, INPUT );
  uBat = nactiVBat();
  serialLogger.log( "accu %.2f V", uBat );

  if( uBat<BAT_LIMIT_2 && uBat>1.0 )  {
    serialLogger.log( "malo baterky %.1f", uBat);
    setDeepSleepTimer( 1800 );
    startDeepSleep();
  }

  pixels.begin(); 
  pixels.setBrightness(brightness);
  pixels.show();  

  signalizujStavBaterky();

  // defaultní jas je plný
  brightness=255;
  brightnessChange = true;

  // bude spousteno periodicky
  tasker.setInterval( doMemoryInfo, 60000 );
  doMemoryInfo();

  tasker.setInterval(nactiBaterku, 1000 );

  // rozsviti posledne pouzity rezim
  loadConfigData();

  // pokud se nic nestane, za 3 min vypneme wifi
  tasker.setTimeout( vypniWifi, 180000 );
}


/** 
 * Volá se taskerem 3 minuty po startu nebo minutu po stisku tlačítka.
 * Pokud není připojený žádný wifi klient, vypne wifi AP = ušetří 60 mA spotřeby.
 * Pokud je připojený, odloží vypnutí o další 2 minuty.
 */
void vypniWifi() {
  if( wifirunner.apNumClients != 0 ) {
    serialLogger.log( "nekdo je pripojen k wifi, necham ho zatim bezet");
    tasker.setTimeout( vypniWifi, 120000 );
    return;
  }

  serialLogger.log( "vypinam WiFi" );
  wifirunner.stopAp();
}



/**
 * Callback z webserveru.
 * Zde nastavte routy pro svou aplikaci.
 * Je zavolano z webserveru v dobe volani webserver.startWebserverOnApAndClient()
 * 
 * Pokud chcete nektere sluzby obsluhovat jen na AP rozhrani, nebo jen na client rozhrani, pouzijte filtry
 *    ON_AP_FILTER
 *    ON_STA_FILTER
 * viz filterApOnly()
 */
void userRoutes( AsyncWebServer * server )
{
  server->on("/", HTTP_GET, onRequestStatus );
  server->on("/rezim0", HTTP_GET, onRequestRezim0 );
  server->on("/rezim1", HTTP_GET, onRequestRezim1 );
  server->on("/rezim2", HTTP_GET, onRequestRezim2 );
  server->on("/rezim3", HTTP_GET, onRequestRezim3 );
  server->on("/fixwifi", HTTP_GET, onRequestFixWifi );
}

/**
 * Callback pro reporting WiFi.
 * Je zavoláno pokaždé, když Wifi přejde z nepřipojeno do připojeno.
 * Pokud se nepoužívá AP+STA nebo STA režim (je použito startAp), tak se nikdy nepoužijí; ale musí být definované.
 */
void WifiStatus_Connected( const char * ssid, int rssi, int channel ) {
}

/**
 * Callback pro reporting WiFi.
 * Je zavoláno pokaždé, když Wifi přejde z připojeno do nepřipojeno.
 * Pokud se nepoužívá AP+STA nebo STA režim (je použito startAp), tak se nikdy nepoužijí; ale musí být definované.
 */
void WifiStatus_NotConnected( int status ) {
}

/** 
 * Callback. Je zavolán tehdy, pokud se připojí klient k AP a dostane IP adresu.
 * Ve wifirunner->apNumClients je počet souběžně připojených klientů.
 * Pokud se nepoužívá AP nebo AP+STA, tak se nikdy nezavolá, ale musí být definované.
 */
void WifiStatus_ClientConnected(char *mac, char *ip)
{
}

/** 
 * Callback. Je zavolán tehdy, pokud se odpojí klient od AP.
 * Ve wifirunner->apNumClients je počet souběžně připojených klientů.
 * Pokud se nepoužívá AP nebo AP+STA, tak se nikdy nezavolá, ale musí být definované.
 */
void WifiStatus_ClientDisconnected(char *mac)
{
  if( vypinatWifi ) {
    tasker.setTimeout( vypniWifi, 120000 );
  }  
}

// ----------- vlastni vykonna cast aplikace (loop)

/** 
 * Voláno z taskeru každých 60 sec.
 * Vypíše stav heapu a změnu odminule - moc příjemné pro hledání memory leaků.
 */
void doMemoryInfo() {
  memoryHelper.printFreeHeap();
}


/** nacte konfiguraci ze souboru */
void loadConfigData() {
  
  // const char * p = config.getString( "limitBaterka", "12.3" ); 
  // limitBaterka = atof( p );
  // Serial.printf( "limitBaterka = %f\n", limitBaterka);

  aktualniRezim = (int) config.getLong( "rezim", 1 );
  color = (int) config.getLong( "color", 0xffffff );
  brightness = (int) config.getLong( "jas", 128 );

  serialLogger.log( "Nacten rezim %d", aktualniRezim );

  if( aktualniRezim==1 ) {
    tasker.setTimeout(fireAction, 1 );
  } else if( aktualniRezim==2 ) {
    int signalMode = (int) config.getLong( "signalMode", 1 );
    signalZacni( signalMode );
  } else if( aktualniRezim==3 ) {
    char morseText[200];
    strncpy( morseText, config.getString( "morseText", "abcd"), 199 );
    morseText[199] = 0;
    int morseSpeed =  (int) config.getLong( "morseSpeed", 100 );
    morseZacni( morseText, morseSpeed );
  }
}

/** ulozi konfiguraci do souboru */
void saveConfigData() {
  char buf[20];

  sprintf( buf, "%d", aktualniRezim );
  config.setValue( "rezim", buf );
  
  sprintf( buf, "%d", color );
  config.setValue( "color", buf );

  sprintf( buf, "%d", brightness );
  config.setValue( "jas", buf );
}



float nactiVBat()
{
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db );
  float u1 = (float)analogReadMilliVolts( BATTERY_PIN ) / 1000.0;
  return u1 / (4.61/(9.7+4.61)) * (3.56/3.24);
}


void nactiBaterku() 
{
  float f = nactiVBat();
  // obcas to bez duvodu vraci nulu
  if( f<0.1 ) return;
  uBat = uBat*0.9 + f*0.1;
  serialLogger.log( "accu %.2f -> %.2f V", f, uBat );
}






void zhasni() {
  pixels.clear();
  pixels.show();
}

void rozsvit() {
  for(int i=0; i<NUMPIXELS; i++) {
      pixels.setPixelColor(i,(color&0xff0000)>>16,(color&0xff00)>>8, color&0xff);
    }
    pixels.show();
}

void doRestart()
{
    ESP.restart();
}




long lastTlacitkoEvent = 0;
#define TLACITKO_SKIP_EVENTS 250

/**
Stav tlačítka. Opakovanými stisky se posouvá v režimech 1-6.
 * 0 = nic (zatím nezmáčknuto)
 * 1 = ohen 100%
 * 2 = ohen 12%
 * 3 = bila 100%
 * 4 = bila 12%
 * 5 = cervena 100%
 * 6 = zelena 100%
 */
int modTlacitkem = 0;

/**
 * Přepíná režimy tlačítkem.
 * 
 * Každé zmáčknutí tlačítka také zapne WiFi na dalších 60 sekund 
 * (pokud se někdo připojí, WiFi pak bude žít tak dlouho, dokud je klient připojený).
 */
void tlacitkoZmacknuto() {
  lastTlacitkoEvent = millis();
  modTlacitkem++;
  if( modTlacitkem==7 ) {
    modTlacitkem=1;
  }
  serialLogger.log( "tlacitko, mod %d", modTlacitkem );

  clearTimers();

  switch( modTlacitkem ) {
    case 1:
      serialLogger.log( "ohen 100%" );
      aktualniRezim = 1;
      brightness=255;
      pixels.setBrightness(brightness);
      tasker.setTimeout(fireAction, 1 );
      break;
    case 2:
      serialLogger.log( "ohen 12%" );
      aktualniRezim = 1;
      brightness=32;
      pixels.setBrightness(brightness);
      tasker.setTimeout(fireAction, 1 );
      break;
    case 3:
      serialLogger.log( "bila 100%" );
      aktualniRezim = 2;
      color = 16777215;
      brightness=255;
      pixels.setBrightness(brightness);
      signalZacni( 1 );
      break;
    case 4:
      serialLogger.log( "bila 12%" );
      aktualniRezim = 2;
      color = 16777215;
      brightness=32;
      pixels.setBrightness(brightness);
      signalZacni( 1 );
      config.setValue( "signalMode", "1" );
      break;
    case 5:
      serialLogger.log( "cervena 100%" );
      aktualniRezim = 2;
      color = 16711680;
      brightness=255;
      pixels.setBrightness(brightness);
      signalZacni( 1 );
      config.setValue( "signalMode", "1" );
      break;
    case 6:
      serialLogger.log( "zelena 100%" );
      aktualniRezim = 2;
      color = 65280;
      brightness=255;
      pixels.setBrightness(brightness);
      signalZacni( 1 );
      config.setValue( "signalMode", "1" );
      break;
  }
  saveConfigData();

}

void loop() {

  if( brightnessChange ) {
    serialLogger.log( "jas %d", brightness );
    pixels.setBrightness(brightness);
    brightnessChange = false;
  }

  if( aktualniRezim==0 ) {
    if( rozsviceno ) {
      serialLogger.log( "zhasinam" );
      zhasni();
      rozsviceno = false;
    }
  } else {
    rozsviceno = true;
  }

  if( millis()-lastTlacitkoEvent > TLACITKO_SKIP_EVENTS ) {
    int tlacitko = digitalRead( TLACITKO );
    if( tlacitko!=stavTlacitka ) {
      stavTlacitka = tlacitko;
      if( stavTlacitka==LOW ) {
        tlacitkoZmacknuto();
      }
    }
  }

  // pokud jedeme z USB, je uBat=0
  if( (uBat>1.0) && (uBat < BAT_LIMIT_1) ) {
    if( aktualniRezim!=0 ) {
      serialLogger.log( "Malo baterky, zhasinam" );
      clearTimers();
      aktualniRezim=0;
      rozsviceno = true;
    }
  }

  if( (uBat>1.0) && (uBat < BAT_LIMIT_2) && !shutdownStarted ) {
    serialLogger.log( "restart" );
    // aby se mezi tim stihly zhasnout svetla, pokud sviti
    tasker.setTimeout( doRestart, 2000 );
    shutdownStarted = true;
  }

  // vypiseme asynchronni log, pokud v nem neco je
  asyncLogger.dumpTo( &Serial );

  // aby Tasker spoustel ulohy
  tasker.loop();

  // odbavit wifi a DNS pozadavky
  wifirunner.process();
  webserver.process();

  // pokud se zmenila konfigurace, ulozit ji do souboru
  if( config.isDirty() ) {
    configProvider.saveConfig();
  }
}


// --------------- oheň --------------------------------------

// https://codebender.cc/sketch:271084#Neopixel%20Flames.ino

void fireAction()
{
 //  Uncomment one of these RGB (Red, Green, Blue) values to
  //  set the base color of the flame.  The color will flickr
  //  based on the initial base color
  
  //  Regular (orange) flame:
    // int r = 226, g = 121, b = 35;
    int r = 226, g = 100, b = 10;
      
  //  Purple flame:
  //  int r = 158, g = 8, b = 148;

  //  Green flame:
  //int r = 74, g = 150, b = 12;

  //  Flicker, based on our initial RGB values
  for(int i=0; i<pixels.numPixels(); i++) {
    int flicker = random(0,70);
    int r1 = r-(flicker/2);
    int g1 = g-flicker;
    int b1 = b-flicker;
    if(g1<0) g1=0;
    if(r1<0) r1=0;
    if(b1<0) b1=0;
    pixels.setPixelColor(i,r1,g1, b1);
  }
  pixels.show();

  //  Adjust the delay here, if you'd like.  Right now, it randomizes the 
  //  color switch delay to give a sense of realism
  tasker.setTimeout(fireAction, random(20,150) );
}


// --------------- morseovka --------------------------------------


const char * morseCharacters[] PROGMEM = {
    ".-",
    "-...",
    "-.-.",
    "-..",
    ".",
    "..-.",
    "--.",
    "....",
    "..",
    ".---",
    "-.-",
    ".-..",
    "--",
    "-.",
    "---",
    ".--.",
    "--.-",
    ".-.",
    "...",
    "-",
    "..-",
    "...-",
    ".--",
    "-..-",
    "-.--",
    "--.."
  };

const char * morseNumbers[] PROGMEM = {
    ".----",
    "..---",
    "...--",
    "....-",
    ".....",
    "-....",
    "--...",
    "---..",
    "----.",
    "-----"
  };

/* czech dialect of Morse code contains character "ch" */
const char * morseLiteral_Ch PROGMEM = "----";

int morseSpeed;
char morseText[200];

int nextCharOffset = 1;

/** pozice ve zdrojovem textu */
int morseSrcPos;
/** pozice v codetab aktualniho znaku */
int morseInCharPos;

bool vysilamZnacku;

int pocetVysilani;

void morseZacni( char * text, int speed ) {
  if( strlen(text)==0 ) {
    strcpy( morseText, "SOS" );
  } else {
    strcpy( morseText, text );
  }
  strlwr( morseText );
  morseSpeed = speed;
  serialLogger.log( "+++ morse: [%s] speed=%d", morseText, morseSpeed);

  morseSrcPos = 0;
  morseInCharPos = 0;
  pocetVysilani = 0;
  vysilamZnacku = false;
  nextCharOffset = 1;

  tasker.setTimeout(morseAction, 1 );
}

#define BASE_SPEED_CARKA 1000
#define BASE_SPEED_TECKA 250
#define BASE_SPEED_PAUZA 1000
#define BASE_SPEED_MEZERA 3000
#define BASE_SPEED_KONEC 6000

void doDelay( int baseDelay ) {
  double d = (double)baseDelay;
  double factor = ((double)morseSpeed) / 100.0;
  d = d / factor;
  tasker.setTimeout(morseAction, (long)d );
}

char logBuffer[100];



void posliJednuZnacku( char * table ) {
  if( vysilamZnacku ) {
    // nedelam nic, pauza po znacce
    serialLogger.log( "%s mezera", logBuffer );
    vysilamZnacku = false;
    zhasni();
    doDelay( BASE_SPEED_PAUZA );
    return;
  }

  char znacka = table[morseInCharPos];
  if( znacka==0 ) {
    serialLogger.log( "%s konec", logBuffer );
    morseSrcPos+=nextCharOffset;
    morseInCharPos = 0;
    zhasni();
    doDelay( BASE_SPEED_MEZERA );
    return;
  }

  vysilamZnacku = true;
  if( znacka=='.' ) {
    serialLogger.log( "%s *", logBuffer );
    morseInCharPos++;
    rozsvit();
    doDelay( BASE_SPEED_TECKA );
    return;
  }
  if( znacka=='-' ) {
    serialLogger.log( "%s --", logBuffer );
    morseInCharPos++;
    rozsvit();
    doDelay( BASE_SPEED_CARKA );
    return;
  }

  serialLogger.log( "CHYBA, tady nemam byt");
}



void morseAction() 
{
  char * pos = morseText + morseSrcPos;
  if( *pos == 0 ) {
    serialLogger.log( "M: konec, zacinam znovu");
    pocetVysilani++;
    morseSrcPos = 0;
    morseInCharPos = 0;
    vysilamZnacku = false;
    doDelay( BASE_SPEED_KONEC );
    return;
  } 
  if( *pos == ' ' || *pos == ',' || *pos == '.' ) {
    serialLogger.log( "M: mezera");
    morseSrcPos++;
    doDelay( BASE_SPEED_MEZERA );
    return;
  }
  if( *pos=='c' && *(pos+1)=='h' ) {
    // vysilame ch
    nextCharOffset=2;
    sprintf(logBuffer, "M: ch %d", morseInCharPos );
    char * table = (char*)morseLiteral_Ch;
    posliJednuZnacku( table );
    return;
  }
  if( *pos>='a' && *pos<='z' ) {
    // vysilame pismeno
    nextCharOffset=1;
    sprintf(logBuffer, "M: %c %d", *pos, morseInCharPos );
    char * table = (char*) morseCharacters[(*pos)-'a'];
    posliJednuZnacku( table );
    return;
  }
  if( *pos>='0' && *pos<='9' ) {
    // vysilame cislo
    nextCharOffset=1;
    sprintf(logBuffer, "M: %c %d", *pos, morseInCharPos );
    char * table = (char*) morseNumbers[(*pos)-'0'];
    posliJednuZnacku( table );
    return;
  }
  // je to nejaky nesmysl
  morseSrcPos++;
}

// --------------- morseovka --------------------------------------

/**
1 stálé světlo
2 blikání
3 pomalé blikání
*/
int signalMode = 1;

bool signalRozsviceno;

#define SIGNAL_STANDARD 1000
#define SIGNAL_SLOW 5000

void signalAction()
{
  if( signalRozsviceno ) {
    zhasni();
    signalRozsviceno = false;
  } else {
    rozsvit();
    signalRozsviceno = true;
  }

  if( signalMode==2 ) {
    tasker.setTimeout(signalAction, SIGNAL_STANDARD );
  } else if( signalMode==3 ) {
    tasker.setTimeout(signalAction, SIGNAL_SLOW );
  }
  // pro rezim 1 se tasker nenastavi = zustane svetlo
}


void signalZacni( int mode ) {
  signalMode = mode;
  signalRozsviceno = false;
  tasker.setTimeout(signalAction, 1 );
}





// -------------- odsud dal je webserver ------------------------------------

/*
 * Je lepe misto Serial.print pouzivat AsyncLogger.
 * Je volano z webserveru asynchronne.
 * Nevolat odsud dlouhotrvajici akce!
 * I logovani by melo byt pres asyncLogger!
 * 
 * Kazda funkce onRequest* musi byt zaregistrovana v userRoutes()
 */


const char hlavicka[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Kouzelná lucerna</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {font-family: Arial; display: inline-block; text-align: left;}
    h2 {font-size: 2.0rem;}
    h3 {font-size: 1.65rem; font-weight: 600}
    p {font-size: 1.4rem;}
    input {font-size: 1.4rem;}
    input#text {width: 100%;}
    select {font-size: 1.4rem;}
    form {font-size: 1.4rem;}
    body {max-width: 600px; margin:10px ; padding-bottom: 25px;}
  </style>
</head>
<body>
<h2>Kouzelná lucerna</h2>
<form method="GET" action="/">
 <input type="submit" name="obnov" value="Obnov stav" > 
</form>
)rawliteral";

const char paticka[] PROGMEM = R"rawliteral(
</body>
</html>
)rawliteral";







void vlozJas( AsyncResponseStream *response ) {
    response->print("<p>Jas: <select name=\"jas\" >");
    response->printf("<option value=\"255\" %s>100 %%</option>", brightness==255 ? "selected" : "" );
    response->printf("<option value=\"128\" %s>50 %%</option>", brightness==128 ? "selected" : "" );
    response->printf("<option value=\"64\" %s>25 %%</option>", brightness==64 ? "selected" : "" );
    response->printf("<option value=\"32\" %s>12 %%</option></select>", brightness==32 ? "selected" : "" );
  }

void vlozBarvu( AsyncResponseStream *response ) {
    response->print("<p>Barva: <select name=\"barva\" >");
    response->printf("<option value=\"16777215\" %s>Bílá</option>", color==16777215 ? "selected" : "" );
    response->printf("<option value=\"16711680\" %s>Červená</option>", color==16711680 ? "selected" : "" );
    response->printf("<option value=\"65280\" %s>Zelená</option>", color==65280 ? "selected" : "" );
    response->printf("<option value=\"255\" %s>Modrá</option></select>", color==255 ? "selected" : "" );
  }




void onRequestStatus(AsyncWebServerRequest *request){
  asyncLogger.log( "* status");

  //Handle Unknown Request
  AsyncResponseStream *response = request->beginResponseStream(webserver.HTML_UTF8);
  response->print( hlavicka );

  int pct = (int) map_double(uBat, BAT_LIMIT_1, 4.2, 0, 100);
  response->printf( "<p>Baterka: %.2f V, %d %%<br>", uBat, pct );

  response->printf( "<p>Režim: %d</p>", aktualniRezim );


  if( aktualniRezim==3 ) {
    response->printf( "<p>Morseovka [<b>%s</b>], pozice %d/%d, pocet odeslani: %d.</p>", morseText, morseSrcPos+1, strlen(morseText), pocetVysilani );
  } 
  if( aktualniRezim==1 ) {
    response->printf( "<p>Oheň</p>" );
  }
  if( aktualniRezim>1 ) {
    const char * barva;
    if( color==16777215 ) {
      barva = "bílá";
    } else if( color==16711680 ) {
      barva = "červená";
    }else if( color==65280 ) {
      barva = "zelená";
    }else if( color==255 ) {
      barva = "modrá";
    } else {
      barva = "???";
    }

    response->printf( "<p>Jas: %d %%, barva: %s</p>", ((brightness+1)*100)/256, barva );
  }

  response->printf( "<p>WiFi se po chvíli vypne: <b>%s</b></p>",
      vypinatWifi ? "ANO" : "NE"
    );

  if( vypinatWifi ) {
    response->print( "<form method=\"GET\" action=\"/fixwifi\"><input type=\"submit\" name=\"obnov\" value=\"Nechat WiFi zapnuté\"></form>" );
  }

const char part1a[] PROGMEM = R"rawliteral(
<hr>
<h3>Nastavení režimu</h3>
  
  <p><a name="0"></a><b>Vypnuto</b></p>

  <form method="GET" action="/rezim0">
  <input type="submit" name="nastav" value="Zhasni lampu" > 
  </form>

<hr width="40%">
  <p><a name="1"></a><b>Oheň</b></p>

  <form method="GET" action="/rezim1">
)rawliteral";

const char part1b[] PROGMEM = R"rawliteral(
  <p><input type="submit" name="nastav" value="Zapal oheň" > 
  </form>
  
<hr width="40%">
  <p><a name="2"></a><b>Signál</b></p>

  <form method="GET" action="/rezim2">
)rawliteral";

  response->print( part1a );
  vlozJas( response );
  response->print( part1b );

    vlozJas( response );
    vlozBarvu( response );

  response->print("<p>Režim: <select name=\"mode\" >");
    response->printf("<option value=\"1\" %s>stálé světlo</option>" , signalMode==1 ? "selected" : "");
	  response->printf("<option value=\"2\" %s>blikání</option>" , signalMode==2 ? "selected" : "");
	  response->printf("<option value=\"3\" %s>pomalé blikání</option>" , signalMode==3 ? "selected" : "");

const char part2[] PROGMEM = R"rawliteral(
</select>
  <p><input type="submit" name="nastav" value="Spusť signál" > 
  </form>  

<hr width="40%">
  <p><a name="3"></a><b>Morseovka</b></p>

  <form method="GET" action="/rezim3">
)rawliteral";

  response->print( part2 );

  response->printf("Text:<br><input type=\"text\" name=\"text\" id=\"text\" value=\"%s\"> ", morseText );
    vlozJas( response );
    vlozBarvu( response );

  	response->print("<p>Rychlost: <select name=\"speed\" > ");
	    response->printf(" <option value=\"200\">2x rychleji</option> ");
	    response->printf(" <option value=\"100\" selected>Standard</option> ");
	    response->printf(" <option value=\"50\">50 %%</option> ");
	    response->printf(" <option value=\"25\">25 %%</option> ");

      response->print("</select> <p><input type=\"submit\" name=\"nastav\" value=\"Vysílej\" ></form>");
  
  response->printf( "<hr><p>Lokalni cas (sec od bootu): %d</p>", time(NULL) );

  response->print( paticka );
  request->send(response);
}

void clearTimers() {
  tasker.setTimeout(morseAction, 0 );
  tasker.setTimeout(fireAction, 0 );
  tasker.setTimeout(signalAction, 0 );
}

void onRequestRezim0(AsyncWebServerRequest *request){
  clearTimers();

  asyncLogger.log( "* rezim 0 - zhasnuto");

  aktualniRezim = 0;
  saveConfigData();

  request->redirect("/#0");
}

void onRequestRezim1(AsyncWebServerRequest *request){
  clearTimers();

  asyncLogger.log( "* rezim 1 - ohen");

  String input;
  if(request->hasParam("jas") ) { 
    input = request->getParam("jas")->value();
    brightness = atol( input.c_str() );
    brightnessChange = true;
  }
  
  aktualniRezim = 1;
  saveConfigData();

  tasker.setTimeout(fireAction, 1 );

  request->redirect("/#1");
}

void onRequestRezim2(AsyncWebServerRequest *request){
  clearTimers();

  asyncLogger.log( "* rezim 2 - blikej");
  
  color = 0xffffff;
  int mode = 1;

  String input;
  if(request->hasParam("jas") ) { 
    input = request->getParam("jas")->value();
    brightness = atol( input.c_str() );
    brightnessChange = true;
  }
  if(request->hasParam("barva") ) { 
    input = request->getParam("barva")->value();
    color = atol( input.c_str() );
  }
  if(request->hasParam("mode") ) { 
    input = request->getParam("mode")->value();
    mode = atol( input.c_str() );
  }
  aktualniRezim = 2;
  
  char buffer[20];
  sprintf( buffer, "%d", mode );
  config.setValue( "signalMode", buffer );
  saveConfigData();

  signalZacni( mode );

  request->redirect("/#2");
}

void onRequestRezim3(AsyncWebServerRequest *request){
  clearTimers();

  asyncLogger.log( "* rezim 3 - morse");

  // text, barva, speed
  char text[200];
  color = 0xffffff;
  int speed = 100;
  strcpy( text, "" );

  String input;
  if(request->hasParam("jas") ) { 
    input = request->getParam("jas")->value();
    brightness = atol( input.c_str() );
    brightnessChange = true;
  }
  if(request->hasParam("text") ) { 
    input = request->getParam("text")->value();
    strncpy( text, input.c_str(), 199 );
    text[199] = 0;
  }
  if(request->hasParam("speed") ) { 
    input = request->getParam("speed")->value();
    speed = atol( input.c_str() );
  }
  if(request->hasParam("barva") ) { 
    input = request->getParam("barva")->value();
    color = atol( input.c_str() );
  }

  char buffer[20];
  config.setValue( "morseText", text );
  sprintf( buffer, "%d", speed );
  config.setValue( "morseSpeed", buffer );

  aktualniRezim = 3;
  saveConfigData();

  morseZacni( text, speed );

  request->redirect("/#3");
}

void onRequestFixWifi(AsyncWebServerRequest *request){
  asyncLogger.log( "@ req fixwifi" );

  vypinatWifi=false;
  tasker.setTimeout( vypniWifi, 0 );

  request->redirect( "/" );
}


/*
ESP32 arduino core 3.3.8

Using library DNSServer at version 3.3.8 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.8\libraries\DNSServer 
Using library ESP32 Async UDP at version 3.3.8 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.8\libraries\AsyncUDP 
Using library WiFi at version 3.3.8 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.8\libraries\WiFi 
Using library Networking at version 3.3.8 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.8\libraries\Network 
Using library Async TCP at version 3.4.10 in folder: E:\dev.moje\arduino\libraries\Async_TCP 
Using library ESP Async WebServer at version 3.11.0 in folder: E:\dev.moje\arduino\libraries\ESP_Async_WebServer 
Using library FS at version 3.3.8 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.8\libraries\FS 
Using library Tasker at version 2.0.3 in folder: E:\dev.moje\arduino\libraries\Tasker 
Using library Adafruit NeoPixel at version 1.15.5 in folder: E:\dev.moje\arduino\libraries\Adafruit_NeoPixel 
Using library SPIFFS at version 3.3.8 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.8\libraries\SPIFFS 
Using library Hash at version 3.3.8 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.8\libraries\Hash 

 */