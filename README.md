# Pixel — webové ovládání LED panelu

ESP32-C3 se 4 MB flash, panel WS2812 16 × 16, GRB / 800 kHz, DIN na GPIO8.
Společná zem panelu a desky. Panel má vlastní 5V zdroj, ESP32 nyní napájí USB.

## Co umí

- Jednotná barva celé matice, jas a vypnutí.
- Kreslení prstem nebo myší do 16 × 16 pixelů, guma, zpět, výplň a vymazání.
- Import/export obrázku jako JSON.
- Běžící text, barva a rychlost; do 64 znaků, automatický převod na velká písmena bez diakritiky.
- Hodiny: dvě číslice hodin nahoře, dvě číslice minut dole. Praha, automatický letní čas.
- Čas z NTP; při nedostupném internetu tlačítko pro seřízení podle mobilu. Po výpadku napájení se čas musí znovu seřídit.
- Uložení aktuální scény včetně kresby a nastavení do flash tlačítkem „Uložit scénu do panelu“.
- Otočení, zrcadlení a volba hadovitého zapojení. Náhled odpovídá logickému obrazu před korekcí zapojení a jasu.

Aktuálně připojeno na **http://192.168.0.44** (DHCP, ověřeno 6. 10. 2026).

## Připojení k domácí Wi-Fi

Síť: `Rocklive_digital_r` (ESP32-C3 potřebuje pásmo 2,4 GHz).
Heslo není součástí zdrojového kódu ani repozitáře.

První nastavení z Macu přes USB:

```sh
python3 scripts/wifi_setup.py
```

Na stejném Macu otevřete `http://127.0.0.1:8766`, zadejte heslo a připojte panel.
Průvodce pošle údaje přes USB, neukládá je do souboru ani je neloguje.
Zobrazí přidělenou IP; tento průvodce pak už není potřeba a lze ukončit Ctrl+C.

Alternativa přímo z mobilu:

1. Připojte se k `LED-Panel-Setup`, heslo `pixel1616`.
2. Otevřete `http://192.168.4.1`.
3. V „Síť a nastavení panelu“ vyplňte domácí Wi-Fi.
4. Po připojení stránka zobrazí domácí IP. Přepněte mobil do domácí sítě a otevřete tuto IP.

Nastavovací AP se vypne dvě minuty po úspěšném připojení.
Při nedostupnosti domácí Wi-Fi se po 25 sekundách znovu otevře.
Přihlašovací údaje ukládá ESP32 do NVS. IP přiděluje router; pro stálou adresu
použijte DHCP rezervaci. Alternativně funguje `http://led-panel.local`, pokud klient podporuje mDNS.
Web je určen pro důvěryhodnou domácí síť; nemá vlastní přihlášení ani internetové publikování.

## Odběr

Výchozí softwarový limit je **1 000 mA** pro panel, nastavitelný v UI od 350 do
4 000 mA (zdroj uživatele je 5 A). Každý snímek se před odesláním přepočítá,
a pokud je potřeba, celý obraz rovnoměrně ztmavne. Nedochází k ořezávání počtu pixelů.

Model počítá s 1 mA klidového odběru na pixel a 20 mA na plně rozsvícenou barevnou složku.
Jde o konzervativní odhad, nikoli měření ani hardwarovou ochranu. Odběr ESP32 není zahrnut.
Při bateriovém napájení nastavte limit podle skutečně dovoleného proudu **na 5V výstupu měniče**.
Vyšší proud odebíraný z baterie při nižším napětí není totéž co proud dodávaný panelu.

Při prvním startu je panel zhasnutý, jas 16/255. Po uložení scény se obnoví uložený stav.

## Soubory a sestavení

- `led_web/led_web.ino`: Wi-Fi, HTTP API, NVS, čas a řízení LED.
- `led_web/matrix_logic.h`: vykreslování písma, mapování a proudový limit.
- `web/index.html`: veškeré HTML/CSS/JS bez CDN a externích zdrojů.
- `led_web/web_ui.h`: generováno ze stránky; neupravovat ručně.
- `led_demo/led_demo.ino`: původní demo zůstává jako záloha zdrojového kódu.
- `backups/original-flash.bin`: původní 4MB flash před prvním demem (soukromá, mimo git).

Použito Arduino ESP32 core **3.3.10** a Adafruit NeoPixel **1.15.2**.
Knihovna je lokálně v `libraries/Adafruit_NeoPixel`; při novém checkoutu ji lze získat:

```sh
git clone --depth 1 --branch 1.15.2 https://github.com/adafruit/Adafruit_NeoPixel.git libraries/Adafruit_NeoPixel
sh scripts/build.sh
sh scripts/upload.sh
```

Průvodce Wi-Fi nebo jiný serial monitor musí být při nahrávání zavřený.

## Ověření

```sh
clang++ -std=c++17 -fsanitize=address,undefined -g tests/test_logic.cpp -o /tmp/led-test
/tmp/led-test
python3 tests/test_device.py http://IP_PANELU
```

Host test kontroluje 10 000 náhodných snímků proti limitu, všechny kombinace otočení/
zrcadlení/zapojení a hranice textového i hodinového rendereru.
Test skutečného zařízení dočasně změní živou scénu a na konci ji obnoví; nic neukládá do flash.
`python3 scripts/preview_server.py` slouží jen pro lokální test webu s **falešným** API, neovládá hardware.

USB serial 115200: `STATUS` vrátí stav bez hesla, `OFF` zhasne panel.

Ověřeno na zařízení 6. 10. 2026: nahrání s ověřením hashe, připojení k domácí Wi-Fi, NTP, všechny čtyři režimy, proudový limit, odmítnutí neplatných vstupů a obnovení živé scény po testu. Mobilní UI ověřeno na šířce 390 px včetně kreslení tažením.
