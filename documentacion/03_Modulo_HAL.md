# 03. HAL-Modul (Hardware Abstraction Layer)

Die Hardware-Abstraktionsschicht (HAL) trennt die Anwendungslogik von den hardwarespezifischen Details des CC2510. Sie liegt im Verzeichnis `src/hal/`.

## 1. Takt (`clock.c`, `clock.h`)
Der CC2510 hat zwei Systemoszillatoren:
*   **RC-OSC (16 MHz / 13 MHz):** interner Oszillator, sparsam, aber ungenau. Beim Einschalten aktiv.
*   **XOSC (24–27 MHz):** externer Quarzoszillator. **Pflicht** für korrekten Betrieb des Radios (RF) und für präzise UART-Baudraten.

### Wichtige Funktionen
*   `init_clock()`: Schaltet den externen Quarz ein, wartet auf Stabilität und wechselt den Systemtakt auf diese Quelle.
    1.  XOSC einschalten (`SLEEP &= ~0x04`).
    2.  Auf das Stabilitäts-Bit warten (`while(!(SLEEP & 0x40))`).
    3.  Taktquelle wechseln (`CLKCON = (CLKCON & ~0x40) | 0x00`).
    4.  RC-Oszillator ausschalten, um Strom zu sparen (`SLEEP |= 0x04`).

## 2. Zeit (`time.c`, `time.h`)
Liefert Werkzeuge zum Messen von Zeit und Erzeugen von Verzögerungen. Nutzt **Timer 1** oder **Timer 3** (je nach Implementierung) bzw. kalibrierte Schleifen.

### Wichtige Funktionen
*   `time_init()`: Konfiguriert den Timer für periodische Interrupts (Tick) bzw. bereitet das Zählen von Millisekunden vor.
*   `delay_ms(uint16_t ms)`: Blockierende Verzögerung von `ms` Millisekunden. Essenziell für Hardware-Initialisierungssequenzen.
*   `millis()`: Gibt die seit Systemstart vergangenen Millisekunden zurück (nützlich für nicht-blockierendes Timing).
*   `sleep_ms(uint32_t ms)`: Versucht, den MCU für eine bestimmte Zeit in den Energiesparmodus (PM1/PM2) zu versetzen.

## 3. LED-Steuerung (`led.c`, `led.h`)
Steuert die an die LEDs angeschlossenen GPIO-Pins.
*   **Typische Pins:**
    *   **Rot:** P1_5 (geteilt mit SPI CLK)
    *   **Grün:** P1_7 (geteilt mit SPI MISO)
    *   **Blau:** P1_6 (geteilt mit SPI MOSI)
    *   **Weiß:** P2_1 (dediziert)

> ⚠️ **Wichtiger Hinweis:** Da die RGB-LEDs sich Pins mit dem SPI-Bus der Flash teilen, können sie nicht gleichzeitig mit Flash-Operationen genutzt werden. Die weiße LED ist in der Regel jederzeit sicher nutzbar.

### Makros
*   `LED_INIT`: Konfiguriert die Pins als Ausgang.
*   `LED_ON` / `LED_OFF` / `LED_TOGGLE`: Steuern die weiße LED.
*   `LED_R_ON`, `LED_G_ON`, `LED_B_ON`: Steuern die RGB-LEDs (mit der genannten Vorsicht).

## 4. UART (`uart.c`, `uart.h`)
Der CC2510 hat zwei USART-Schnittstellen (USART0 und USART1), die als UART oder SPI arbeiten können.
Dieses Modul konfiguriert eine davon (meist USART0 Alt 1 oder Alt 2) für asynchrone serielle Kommunikation.

### Typische Konfiguration
*   **Baudrate:** 115200 (abhängig von `U0BAUD` und `U0GCR`).
*   **Pins:** P0_2 (RX) und P0_3 (TX) in Alt 1.

### Wichtige Funktionen
*   `uart_init()`: Konfiguriert die Register `U0CSR`, `U0GCR`, `U0BAUD` und die Pins `P0SEL`.
*   `uart_write(uint8_t byte)`: Sendet ein Byte und wartet auf den Abschluss der Übertragung.
*   `uart_read(uint8_t *byte)`: Prüft auf empfangene Daten und liest sie.

## 5. Interrupts (`isr.h`)
Definiert Makros und Prototypen zur Behandlung der System-Interrupts (RF, DMA, Timer, I/O).
*   `HAL_ENABLE_INTERRUPTS()`: Aktiviert globale Interrupts (`EA = 1`).
*   `HAL_DISABLE_INTERRUPTS()`: Deaktiviert globale Interrupts (`EA = 0`).
