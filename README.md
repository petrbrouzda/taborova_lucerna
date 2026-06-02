# Kouzelná lucerna na tábor 

Táborová rekvizita - chytrá lucerna.
ESP32 a kousek neopixel led pásku.
Ovládá se přes WiFi nebo tlačítkem.

Umí následující kouzla:
* hořící oheň (nastavitelný jas)
* stálé svícení (nastavitelná barva a jas)
* blikání pomalé či rychlé (nastavitelná barva a jas)
* vysílání morseovky (nastavitelná barva, jas a rychlost)

Pamatuje si poslední nastavený režim; po restartu pokračuje ve stejném režimu.

Předpokládá napájení z lithiového článku, měří napětí akumulátoru a pokud je pod limitem, zhasne led pásek. Pokud je pod ještě nižším limitem, vypne i ESP.
(Pokud není na analogový vstup nic připojeno, běží bez problémů.)

Výdrž na baterii je závislý od spotřeby LED - a tedy od zvoleného režimu svícení a jasu. Na 2000 mAh akumulátor s páskem s 11 LED vydrží při režimu "oheň" se 100% jasem cca 6 hod.

Tlačítkem se přepínají režimy v pořadí:
* oheň, 100% jas
* oheň, 12% jas
* bílá, 100% jas
* bílá, 12% jas
* červená, 100% jas
* zelená, 100% jas

Lucerna nabízí WiFi. WiFi AP se zapne na tři minuty po startu a na minutu po každém dalším zmáčknutí tlačítka. Pokud se žádné zařízení nepřipojí, WiFi se po udané době vypne (šetření baterie); pokud se někdo připojí, WiFi zůstane zapnuté tak dlouho, dokud je klient připojený.

Vypněte si na mobilu data a připojte se k wifi (jméno/heslo je nastaveno v [webserver-config.h](minecraft-lampicka-g2/webserver-config.h)), captive portál vám zobrazí stav zařízení a umožní ho ovládat. Pokud se na vašem telefonu captive portál neotevře, zadejte do prohlížeče adresu 200.200.200.1.

Ovládací portál ukazuje stav baterky, aktuální režim, a umožňuje zapínat jednotlivé operace:

![](/doc/pg1.png)

![](/doc/pg2.png)

---

Schéma zapojení [je zde.](doc/schema.svg)

Potřebné součástky:
* ESP32-C3 supermini - https://s.click.aliexpress.com/e/_c4ME2h5F
* nabíječ a ochrana lithiového článku - https://s.click.aliexpress.com/e/_c3jMULW5
* step-up na 5 V - https://www.laskakit.cz/laskakit-bat-boost-menic-5v-0-6a-dio6605b/
* jedno tlačítko na přepínání režimů
* jeden vypínač
* USB konektor pro nabíjení akumulátoru
* kus LED pásku s WS2812B (na napájení 5 V) 
* lithiový akumulátor - používám válcový 18650

---

Pro sestavení aplikace je potřeba Arduino IDE s nainstalovanou ESP32 core ("deskou") verze 3.3.x (testováno na 3.3.8); s deskou verze 2.x to nebude fungovat.

Nutné knihovny:
* Adafruit NeoPixel (Adafruit) 1.15.5
* Async TCP (by ESP32Async) 3.4.10
* ESP Async WebServer (by ESP32Async) 3.11.0 
* Tasker (by Petr Stehlík) 2.0.3

Konfigurace desky:
* typ desky: ESP32-C3 Dev module
* USB CDC on boot: zapnuto
* CPU speed: 80 MHz
* Flash speed: 40 MHz
* Flash mode: DIO
