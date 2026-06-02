#ifndef __CONFIG___H_
#define __CONFIG___H_

// Konfigurace wifi AP - tj. konfigurační wifi, na kterou se budete případně připojovat mobilkou.
// Za jméno EspCam_ se příkazem .addChipIdToApHostname() doplní ID čipu, tj. EspCam_123456.
#define AP_SSID "LucernaA"
#define AP_PASSWORD "heslo12345"

// Pokud váš mobil nedělá automaticky přesměrování, zadejte do prohlížeče http://200.200.200.1

// Řešení captive portálu pro Samsungy:
// https://github.com/esp8266/Arduino/commit/d5265aa5ac4995b65bce3e29025307cfd7ae0d0f#diff-9978b19703e34e9504dcdda781907935
// Took me quite a while to figure this out, but according to this issue (espressif/arduino-esp32#1037), in order to get 
// a captive notification to show or the popup to open, the DNS server must resolve to a public IP. 
// It will not work with a private one (e.g. 192.168.4.1).
IPAddress local_ip(200,200,200,1);
IPAddress gateway(200,200,200,1);
IPAddress subnet(255,255,255,0);

#define WEB_PORT 80 

// na jakou adresu ma presmerovavat captive portal?
#define CAPTIVE_PORTAL_REDIRECT_REL "/"

#endif