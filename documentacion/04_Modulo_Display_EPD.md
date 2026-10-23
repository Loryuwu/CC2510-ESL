# 04. Display-Modul EPD (Electronic Paper Display)

Dieses Modul (`src/display/epd.c`) steuert das E-Paper-Display. Solche Displays brauchen eine spezielle Abfolge von Spannungen und Befehlen, um sich korrekt und ohne Schaden zu aktualisieren.

## Hardware
*   **Schnittstelle:** SPI (Serial Peripheral Interface).
*   **Controller:** im Glas integriert (COG). Kompatibel mit generischen Pervasive-Displays-Treibern.
*   **Steuerpins:**
    *   **CS (Chip Select):** P0_1
    *   **DC (Data/Command):** P1_2 (LOW = Befehl, HIGH = Datum)
    *   **BUSY:** P1_3 (Eingang, HIGH = beschäftigt)
    *   **RESET:** P2_0
    *   **PWR (Power Enable):** P0_0 (steuert einen Transistor zur Displayversorgung)

## Funktionsweise

### 1. Initialisierung (`epd_init`)
Bereitet das Display auf den Datenempfang vor.
1.  Konfiguriert SPI-Pins und GPIOs.
2.  Schaltet die Versorgung ein (`EPD_PWR_ON`).
3.  Erzeugt einen Hardware-Reset-Puls.
4.  Sendet Soft-Reset-Befehle.
5.  Konfiguriert Parameter wie Temperatur (wichtig für die Tinten-Waveform) und PSR.

### 2. Bildaktualisierung (`epd_globalUpdate`)
Spectra-Displays haben zwei Farbkanäle:
*   **Schwarz/Weiß-Kanal:** definiert die schwarzen und weißen Pixel.
*   **Rot-Kanal:** definiert die roten Pixel.

Die Funktion erhält zwei Zeiger auf Bildpuffer:
```c
void epd_globalUpdate(const uint8_t * data1s, const uint8_t * data2s)
```
1.  Sendet Befehl `0x10`, gefolgt von den S/W-Kanaldaten.
2.  Sendet Befehl `0x13`, gefolgt von den Rot-Kanaldaten.
3.  Ruft `epd_flushDisplay()` auf, um den physischen Refresh zu starten.

### 3. Refresh (`epd_flushDisplay`)
1.  Schaltet den internen DC/DC-Wandler ein.
2.  Sendet den Befehl „Display Refresh" (`0x12` oder `0x11`).
3.  Wartet, bis der **BUSY**-Pin freigegeben wird (Display fertig aktualisiert).
4.  Schaltet den DC/DC-Wandler ab.

### 4. Abschalten (`epd_powerOff`)
Korrektes Abschalten ist kritisch, um Schäden und Stromverbrauch (Ghosting/Einbrennen) zu vermeiden.
1.  Schaltet den DC/DC ab.
2.  Setzt SPI-Bus und Steuerpins in einen sicheren Zustand (LOW).
3.  Trennt die Hauptversorgung (`EPD_PWR_OFF`).

## Datenformat
Das Bild wird als Byte-Array gesendet. Jedes Bit entspricht einem Pixel.
*   **Puffergröße:** abhängig von der Auflösung (z. B. 2,66" = 152×296 Pixel).
*   `BUFFER_SIZE = (HRES / 8) * VRES`
