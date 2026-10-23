# PriceTag — ESL als Home Assistant Display

## Ziel

Imagotag-ESL (Electronic Shelf Labels) mit CC2510 + E-Ink als Home Assistant
Displays nutzen: Wetter, Energie, Status. Zwei Panel-Größen parallel am selben
Gateway:
- **2,66"** 296×152 BWR (IL0373/E2266) — hwType 0x56
- **4,2"**  400×300 BWR (IL0398/E2417JS0D1) — hwType 0x02

## Architektur

```
Home Assistant ──drawcustom──▶ ESP32-S3 Gateway (OEPL-Shim) ──nRF24──▶ CC2510 ──SPI──▶ E-Ink
                                 │ per Tag: hwType → HA rendert passende Auflösung
                                 │ JPEG → BWR-Quantisierung → RLE → Funk-Stream
```

## Stand 2026-10-22: Dual-Display (2,66" + 4,2"), OEPL-Shim, dynamische Konfig

Aktueller, lauffähiger Stand nach der HA-Integration. Die drei Gateway-Builds
liegen in `gateway/`, jeweils als PlatformIO-Projekt (pioarduino-Core
arduino-esp32 3.x / IDF 5.x — der alte, eingefrorene PlatformIO-Core brachte das
WLAN-PHY auf dem ESP32-S3 nicht hoch). Zugangsdaten jeweils in lokaler,
git-ignorierter `credentials.h` (Vorlage: `credentials.h.example`).

**Entscheidende Funk-Erkenntnis:** Das ESP-WLAN stört den direkt benachbarten
nRF24 massiv. Auf 2 m gemessen: WLAN an = RSSI −75 dBm / 67 % Retransmits /
Stream FAIL; WLAN aus = −53 dBm / 15 % / OK. Lösung: Das Gateway schaltet das
WLAN **nur für die Dauer eines Bild-Streams** ab und danach wieder an — Wetter-
Abruf braucht WLAN, der Stream nicht. Es ist **kein** Reichweiten-/Antennen-,
sondern ein Koexistenz-Problem (beide Chips sind reine 2,4-GHz-Hardware).

### `gateway/gateway_weather_pio/` — eigenständiges Wetter-Gateway
Holt Wetter selbst (Open-Meteo), rendert ein 296×152-BWR-Layout (Banner mit
Ort/Uhr, Temp, gefühlt, Hoch/Tief, Feuchte, Wind), streamt es RLE-komprimiert.

### `gateway/gateway_oepl_shim/` — OpenEPaperLink-AP-Emulation (empfohlen)
Der ESP32 gibt sich nach außen als **OEPL-Accesspoint** aus → die OEPL-
Home-Assistant-Integration (`drawcustom`) und der ESPHome-Designer (Drag&Drop)
funktionieren unverändert. **HA rendert das Bild**, der Shim quantisiert das
hochgeladene JPEG zu Schwarz/Weiß/Rot und funkt es per bestehendem RLE-Stream.
- **Dual-Display:** Jeder Tag meldet seinen Panel-Typ beim Enroll (PANEL_TYPE-
  Byte, rückwärtskompatibel). Gateway meldet pro Tag den passenden hwType an HA
  → HA rendert die richtige Auflösung (296×152 oder 400×300). JPEG-Decode und
  Scan-Koordinaten sind panel-abhängig (2,66" = 90°-Rotation, 4,2" = nativ).
- Endpunkte: `GET /`, `/sysinfo`, `/get_db` (hwType 0x56 = 2,66" / 0x02 = 4,2"),
  WS `/ws`, `POST /imgupload`, `/settype?mac=...&type=42|266` (manueller Override).
- NVS-Migration: alte 12-Byte-Einträge werden beim Start automatisch auf 13 Byte
  (+ disp-Feld) migriert, Default = 2,66".
- Erreichbar per DHCP + mDNS `pricetag-ap.local`. Tag-Enroll per NFC-UID.

### Dynamische Konfig (host-gesteuert, eine Protokoll-Erweiterung)
Die Poll-Antwort des Gateways (`NOUPDATE` / `IMG_SHOW`) trägt jetzt
`[sleep_sekunden:2][kanal:1]`. Default = altes Verhalten (300 s, ch76), daher
voll rückwärtskompatibel.
- **Poll-Intervall:** `/config?poll=<s>` (z. B. 900 für Akku-Schonung).
- **Kanal-Agilität + Auto-Scan:** `/scan[?apply=1]` misst per nRF24-RPD den
  ruhigsten Kanal; `/config?ch=<n>` setzt ihn. Gateway sagt dem Tag den neuen
  Kanal in der Poll-Antwort an und wechselt dann selbst; **Home-Kanal 76 =
  fester Rendezvous + Fallback** (verpasst der Tag die Ansage, finden sich beide
  dort wieder). CC-seitig: `radio_set_channel()` + `apply_cfg()`.
- Aktiv auf dem Tag erst nach **Reflash** von `firmware/display.bin` über den
  CC-Debugger; Shim und Tag sind in beide Richtungen kompatibel.

### Display-Hardware-Fakt
Das 3-Farb-Panel (IL0373/E2266) kann **kein Partial-Refresh** — laut Datenblatt
nur „global update", Refresh ~17,4 s. Eine tickende Uhr ist damit physikalisch
nicht möglich; Update alle paar Minuten ist die sinnvolle Kadenz. Das Pull-
Modell (Tag schläft, pollt, zeigt nur bei neuem Bild) ist bereits das
akkuschonende Design (Dauer-RX ~18 mA ≙ Tage; Poll-Schlaf ≙ Monate).

## Funkstrecken-Status (Stand 2026-10-16)

### CC2510 → ESP (nRF24 empfängt): FUNKTIONIERT, verifiziert

Serial zeigt saubere Pakete: `01 08 XX XX DE AD 54 45 53 54 YY` → `VALID pkt tag 0x01, msg=TEST, seq=N`.

Erkenntnisse über das Framing (hart erarbeitet):
- CC2510-Radio braucht den **26 MHz Quarz** (XOSC), nicht den HS-RC-Oszillator. `select_xosc()` in `radio_init`. Ohne Quarz sendet/empfängt das Radio gar nicht.
- Sync-Wort = `A7 5C` (SYNC1/SYNC0), **15/16 Sync-Modus** (16-Bit). So matcht die 3-Byte nRF24-Adresse `A7 5C A7`.
- nRF24 läuft mit **CRC aus** → einfaches ShockBurst, **kein 9-Bit PCF** auf der Empfangsseite. Deshalb darf `radio_tx` die Daten **nicht** bit-shiften: `tx_ota[0]=0xA7` (vervollständigt die Adresse), danach die Protokoll-Bytes direkt.
- Protokoll: `[my_addr][len][daten...][xor]`.

### ESP → CC2510 (CC2510 empfängt): FUNKTIONIERT, gelöst 2026-10-16

Monatelanger „Blocker" per Loopback-Instrument (`gateway_loopback.ino` +
`main_radio_loopback.c`) in vier Schritten geknackt. Es war **nie** die
Funkphysik — Sync lockt die ganze Zeit. Zwei echte Bugs:

1. **FIFO-Overflow durch fehlenden Gleichschritt.** `RFTXRXIF` (TCON.1) ist
   ein Ereignis *pro über-Luft-Byte*, kein Level-„Byte-im-FIFO"-Flag — genau
   wie beim TX taktet das Radio die CPU. Der alte RX-Loop pollte mit
   `delay_ms(1)` zwischen Bytes; bei 250 kbps (~32 µs/Byte) rauscht nach dem
   ersten Byte das ganze Paket durch → RXOVF → danach keine Events mehr. Fix:
   reiner Tight-Spin ohne `delay_ms` über den ganzen RX-Pfad (`radio_rx_raw`).

2. **nRF24-Adresse reversed.** Der nRF24 sendet Multi-Byte-Adressen in
   umgekehrter Byte-Reihenfolge auf die Luft. Die alte 3-Byte-Adresse
   `A7 5C A7` ist ein Palindrom → fiel nie auf. Die 5-Byte-TX-Adresse musste
   von `{AA,AA,A7,5C,A7}` auf `{A7,5C,A7,AA,AA}` gedreht werden, damit on-air
   `AA AA A7 5C A7` entsteht (AA AA = echte Extra-Preamble vor dem Sync).

Verifiziert: CC-Capture ist sauber `A7 01 06 50 49 4E 47 ..` ("PING") mit
passender XOR. `radio_rx()` ist final gefixt (Lockstep + byte-aligned Decode,
Offset 1, kein Shift). Gelegentlich kippt ein einzelnes Bit → die
XOR-Prüfsumme verwirft solche Pakete (ggf. später Retransmit/FEC ergänzen).

## Hardware

| Teil          | Chip/Modul       | Anschluss                      |
|---------------|------------------|--------------------------------|
| ESL Tag       | CC2510F32        | SoC mit 2.4GHz Radio           |
| E-Ink 2,66"   | IL0373 (E2266)   | 296×152 BWR, SPI (P0.0=PWR, P0.1=CS, P1.2=DC, P1.3=BUSY, P2.0=RST) |
| E-Ink 4,2"    | IL0398 (E2417JS0D1) | 400×300 BWR, identisches SPI-Pinout |
| Gateway       | ESP32-S3 + nRF24L01+ | SPI (CE=1, CSN=2, SCK=3, MISO=9, MOSI=7) |
| Debugger      | ESP32-S3 (CCLib) | CC-Debug-Protokoll zum Flashen  |

Displays: 3 Farben (schwarz, weiß, rot), identische SPI-Verdrahtung.
CC2510-Firmware: `make display` → 2,66", `make display42` → 4,2" (compile-time `-DPANEL_42`).

## Projektstatus

### Fertig

- [x] CC2510 Pinout + Hardware reverse-engineered
- [x] CC-Debugger auf ESP32-S3 portiert (CCLib_ESP32S3/)
- [x] Flash schreiben/lesen/löschen funktioniert
- [x] EPD-Treiber (epd.c) — Text (5x7 Font skalierbar), Rechtecke, 3 Farben
- [x] Radio-Modul (radio.c) — CC2510 TX/RX kompatibel mit nRF24L01+
- [x] nRF24 Gateway empfängt CC2510-Pakete (carrier_test bestätigt)
- [x] CC2510 empfängt nRF24-Pakete (ESP→CC gelöst 2026-10-16, radio_rx() funktioniert)
- [x] Imagotag-Beacon-Protokoll auf ch76 beobachtet (~113ms Intervall)

### Phase 1: ESP32 → CC2510 → Display (ohne HA)

Ziel: ESP32 Gateway schickt Zeichenkommandos per Funk, CC2510 zeigt sie an.

**CC2510 Firmware (firmware/src/main_display.c)** — gebaut 2026-10-16, HW-Test offen

- [x] Hauptschleife: Radio RX → Kommandos parsen → Display-Liste → epd_update
- [x] Kommando-Protokoll CLEAR / RECT / TEXT / UPDATE / PING / ACK
- [x] Text-Pool (epd_text speichert nur Pointer → eingehenden Text persistent kopieren)
- [x] EPD-Init vor radio_init (RC-Takt-Timing), Panel bleibt an (kein sleep)
- [ ] (später) Auto-Update bei Kommando-Timeout; Deep-Sleep + Wake (Phase 3)

**ESP32 Gateway (gateway/gateway_display/)** — gebaut 2026-10-16, HW-Test offen

- [x] nRF24 TX an CC2510 (Array {A7,5C,A7,AA,AA} → on-air AA AA A7 5C A7)
- [x] Kommandos als Byte-Pakete (1 Kommando/Paket, 40ms Abstand)
- [x] Demo: HA-Layout (Wetter/Energie/Status) + Zähler, alle 10s neu
- [x] PING/ACK-Handshake + UPDATE/ACK (wartet auf Refresh-Bestätigung)

Build: `make display` → `firmware/display.bin` (2,66"), `make display42` → `firmware/display42.bin` (4,2").
Flash: `python CCLib/Python/cc_write_direct.py <COM> firmware/display[42].bin`.
Gateway: `gateway/gateway_display/gateway_display.ino` auf ESP32 flashen.

### Phase 2: Home Assistant Anbindung

- [ ] ESP32: WiFi + MQTT Client
- [ ] HA: MQTT Automation schickt Wetter/Energie/Status als JSON
- [ ] ESP32: JSON → Zeichenkommandos → Funk
- [ ] Periodisches Update (z.B. alle 5 Minuten)

### Phase 3: Erweiterungen (optional)

- [ ] Icons/Bitmaps für Wettersymbole
- [ ] Batteriestatus vom Tag zurückmelden
- [ ] Mehrere Tags unterstützen (verschiedene Adressen)
- [ ] Deep-Sleep mit Wake-on-Radio
- [ ] OTA Firmware Update über Funk

## Kommando-Protokoll (Entwurf)

Jedes Paket: max 29 Byte über radio.c Protokoll (addr + len + data + xor).

```
Byte 0: Kommando-ID
Byte 1+: Parameter

CMD_CLEAR  = 0x01  [color:1]
CMD_RECT   = 0x02  [x_hi:1][x_lo:1][y:1][w_hi:1][w_lo:1][h:1][color:1]
CMD_TEXT   = 0x03  [x_hi:1][x_lo:1][y:1][scale:1][color:1][string:N]  (max ~23 Zeichen)
CMD_UPDATE = 0x10  []
CMD_PING   = 0xF0  []
CMD_ACK    = 0xF1  [status:1]
```

Ablauf:
1. Gateway sendet CMD_CLEAR
2. Gateway sendet N × CMD_TEXT / CMD_RECT
3. Gateway sendet CMD_UPDATE
4. CC2510 rendert + refresht Display
5. CC2510 sendet CMD_ACK zurück

## Verzeichnisse

```
PriceTag/
├── firmware/          CC2510 Firmware (SDCC)
│   ├── src/
│   │   ├── main.c              Aktuell: statisches Demo-Bild
│   │   ├── main_radio_test.c   Radio TX Test
│   │   ├── main_radio_rx_test.c Radio RX Test
│   │   ├── epd.c / epd.h       E-Ink Treiber
│   │   ├── radio.c / radio.h   CC2510 Radio ↔ nRF24
│   │   ├── cc2510.h            Register-Definitionen
│   │   └── delay.c             Delay-Funktion
│   └── Makefile
├── gateway/           ESP32-S3 Arduino Sketches
│   ├── carrier_test/  Diagnose + Paket-Empfang
│   ├── channel_scan/  Kanal-Scanner
│   ├── gateway_rx_test/  RX Test
│   └── gateway_tx_test/  TX Test
├── CCLib/             CC-Debugger (Original + CC2510 Support)
├── CCLib_ESP32S3/     CC-Debugger auf ESP32-S3
├── cc2510.pdf         Datasheet
└── 1P228...pdf        E-Ink Display Datasheet
```
