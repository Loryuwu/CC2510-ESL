# CC2510-ESL / PriceTag — ESL als Home-Assistant-Display

> **ℹ️ Herkunft & Attribution:** Dieses Repo ist ein **Fork von [Loryuwu/CC2510-ESL](https://github.com/Loryuwu/CC2510-ESL)** (das wiederum auf [@andrei-tatar](https://github.com/andrei-tatar/imagotag-hack) aufbaut). Es enthält eine **eigenständige Weiterentwicklung** („PriceTag") der Firmware und ein ESP32-Gateway, mit dem ein CC2510-Preisschild als **Home-Assistant-Display** über Funk läuft. Aus dem ursprünglichen Fork übernommene Module sind als solche gekennzeichnet; alle Credits siehe [unten](#-credits).
>
> 🇪🇸 Spanisches Original-README des Ur-Forks: [README.es.md](./README.es.md) · 📄 ausführliche Projektnotizen: [PROJECT.md](./PROJECT.md)

Ein Imagotag-/Vusion-ESL mit **CC2510** + **IL0373-E-Ink** (296×152, 3 Farben) zeigt Wetter, Energie und Status aus Home Assistant an.

## 🧭 Architektur

```
Home Assistant ──(MQTT/WiFi)──▶ ESP32-S3 + nRF24L01+PA/LNA ──(250 kbps, ch76)──▶ CC2510 ──SPI──▶ E-Ink
```

Das Gateway rendert das komplette Bild (Adafruit GFX), komprimiert es per RLE und streamt es zuverlässig (seq/ACK/Retransmit) zum Tag. Der Tag schläft zwischen Updates (PM2) und pollt das Gateway alle 5 Minuten — das E-Ink hält das Bild stromlos.

## 📊 Status

- ✅ CC2510-Radio **nRF24-kompatibel** (GFSK 250 kbps, ch76, AFC, CRC-8) — beide Funkrichtungen verifiziert
- ✅ E-Ink-Treiber mit Font/Icons + **RLE-Vollbild-Streaming**
- ✅ **NFC-Identität** (UID + Label) + **Enrollment** (Gateway vergibt Adresse)
- ✅ **Deep-Sleep (PM2)** mit 5-Min-Poll-Zyklus
- ✅ ESP32-Gateway mit **Wetter** (Open-Meteo), Enroll-Registry, Link-Metriken
- ✅ **CC-Debugger auf ESP32-S3 portiert** (Flashen ohne dedizierten Debugger)
- ⏳ offen: direkte **Home-Assistant-MQTT-Anbindung** (aktuell holt das Gateway das Wetter selbst) — siehe [PROJECT.md](./PROJECT.md), Phase 2

## 🗂️ Projektstruktur

```text
CC2510-ESL/
├── firmware/             # CC2510-Firmware (SDCC)
│   ├── src/
│   │   ├── radio.c/.h     nRF24-kompatibles Radio (AFC, CRC-8)
│   │   ├── epd.c/.h       IL0373-Treiber (Font, Icons, RLE-Streaming)
│   │   ├── nfc.c/.h       NFC-Identität (UID/Label)
│   │   ├── icons.c/.h, delay.c, cc2510.h
│   │   ├── main_display.c Haupt-Node (Enroll + Bildempfang + Sleep)
│   │   ├── main_*.c       Test-/Diagnose-Builds (radio, nfc, enroll …)
│   │   ├── flash.c/.h     ⤷ aus Fork übernommen: W25X10 SPI-Flash
│   │   ├── uart.c/.h      ⤷ aus Fork übernommen: UART (Debug)
│   │   ├── cobs.c/.h      ⤷ aus Fork übernommen: COBS-Framing
│   │   ├── hal_compat.h   Shim, damit die Fork-Module hier kompilieren
│   │   └── main_flash_test.c / main_cobs_test.c   Demos der Fork-Module
│   └── Makefile
├── gateway/              # ESP32-S3 Arduino-Sketches (nRF24)
│   ├── gateway_weather/  Haupt-Gateway (Wetter, Vollbild-Render + Stream)
│   └── …                 Diagnose/Tests (carrier, channel_scan, loopback …)
├── tools/
│   ├── CCLib_ESP32S3/    CC-Debugger auf ESP32-S3 (Flashen)
│   └── CCLib/            CC-Debugger-Lib (mit CC2510-Support)
├── doc/                  # Datenblätter (CC2510, NFC, EPD, Flash, Boost)
├── documentacion/        # Modul-Doku (DE) — beschreibt die Fork-/Zusatzmodule
├── img/ · cobs_test.py · PROJECT.md · README.md · LICENSE
```

## 🔧 Firmware bauen (CC2510, SDCC)

```bash
cd firmware
make display       # Haupt-Node (Enroll + Bildempfang)
make flash-test    # Demo: externen SPI-Flash erkennen + ID aufs Display
make cobs-test     # Demo: UART+COBS-Echo (PC-Gegenstelle: cobs_test.py)
make clean
```
Weitere Targets: `radio-tx`, `radio-rx`, `radio-loopback`, `nfc-scan`, `nfc-uid`, `nfc-test`, `nfc-dump`, `enroll`.

### Übernommene Zusatzmodule (flash / uart / cobs)
Aus dem ursprünglichen Loryuwu-Fork wurden **SPI-Flash**, **UART** und **COBS** in die Firmware verdrahtet (angepasst via `hal_compat.h`, Demos `make flash-test` / `make cobs-test`).
> ⚠️ **Pin-Konflikte:** Der SPI-Flash teilt sich USART1 **und** die Pins mit den RGB-LEDs; UART (P0_4/P0_5) teilt sich die Pins mit NFC-SDA bzw. EPD-CLK. Flash, UART und der Display-Node laufen daher nicht alle gleichzeitig — die Module sind für eigene Builds/Debug gedacht.

## ⚡ Flashen (ohne CC-Debugger)
Via ESP32-S3 als Debugger (`tools/CCLib_ESP32S3/`), dann vom PC:
```bash
python tools/CCLib/Python/cc_write_direct.py <PORT> firmware/display.bin
```

## 📡 Gateway (ESP32-S3)
`gateway/gateway_weather/gateway_weather.ino` in der Arduino-IDE öffnen, Bibliotheken „Adafruit GFX" + „RF24" (TMRh20) installieren, oben **WLAN/Standort eintragen** (Platzhalter), flashen.
> ⚠️ **Keine echten Zugangsdaten committen** — die Felder im Sketch sind absichtlich Platzhalter (`.gitignore` blockt zusätzlich `secrets.h`).

## 🙏 Credits
- **[Loryuwu](https://github.com/Loryuwu)** — Ur-Fork [CC2510-ESL](https://github.com/Loryuwu/CC2510-ESL); daraus stammen die Zusatzmodule flash/uart/cobs und die EPD-/NFC-Grundlagen.
- **[@andrei-tatar](https://github.com/andrei-tatar/imagotag-hack)** — Basis-Reverse-Engineering, auf dem das alles aufbaut.
- **CCLib** — CC-Debugger-Bibliothek (siehe `tools/CCLib/LICENSE`).

## 📄 Lizenz
[MIT](./LICENSE) für die eigenen Beiträge, mit Hinweis auf das unlizenzierte Ur-Projekt. `tools/CCLib` behält seine eigene Lizenz.
