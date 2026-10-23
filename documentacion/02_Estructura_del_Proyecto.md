# 02. Projektstruktur

Dieses Dokument beschreibt die Organisation der Dateien und Verzeichnisse und den Zweck jeder Komponente.

## Verzeichnisbaum
```
CC2510-ESL/
├── Makefile                # Build-Skript (plattformübergreifend)
├── README.md               # Kurzdokumentation (Deutsch)
├── README.es.md            # spanisches Original
├── cobs_test.py            # Python-Skript zum Testen der seriellen Kommunikation
├── firmware.hex            # kompiliertes Binary (Ausgabe)
├── src/                    # Firmware-Quellcode
│   ├── main.c              # Einstiegspunkt + Demo-Auswahl
│   ├── hal/                # Hardware Abstraction Layer (HAL)
│   │   ├── clock.c/h       # Systemtakt-Konfiguration
│   │   ├── hal.h           # allgemeine Makros und Definitionen
│   │   ├── isr.h           # Interrupt-Definitionen
│   │   ├── led.c/h         # LED-Steuerung
│   │   ├── time.c/h        # Zeitfunktionen (delay, millis)
│   │   └── uart.c/h        # serieller Treiber
│   ├── display/            # Display-Treiber
│   │   └── epd.c/h         # E-Paper-Controller
│   ├── flash/              # Speicher-Treiber
│   │   └── flash.c/h       # Treiber für externe SPI-Flash
│   ├── rf/                 # Radio-Treiber
│   │   └── rf.c/h          # RF-Transceiver-Treiber
│   ├── cobs/               # Kommunikationsprotokolle
│   │   └── cobs.c/h        # COBS-Kodierung
│   └── image_data/         # statische Daten
│       └── image.h         # Byte-Arrays mit Testbildern
├── doc/                    # technische Doku und Datenblätter
└── img/                    # Bilder für die Dokumentation
```

## Modulbeschreibung

### 1. `src/main.c`
Die Hauptdatei der Firmware. Enthält die Funktion `main()`, die:
1.  Takt und Peripherie (HAL) initialisiert,
2.  Testsequenzen ausführt (über das Makro `DEMO` ausgewählt),
3.  die Endlosschleife (`while(1)`) mit der Anwendungslogik enthält (z. B. auf RF-Pakete warten, Display aktualisieren).

### 2. `src/hal/` (Hardware Abstraction Layer)
Enthält die Low-Level-Treiber, die direkt mit den Registern des CC2510 arbeiten.
*   **clock**: Konfiguriert die Oszillatoren. Der CC2510 startet mit einem internen RC-Oszillator (ungenau) und muss für RF und UART auf den externen Quarz umschalten.
*   **led**: Abstrahiert die Steuerung der LED-Pins.
*   **uart**: Konfiguriert den seriellen Port (Baudrate, Pins) und sendet/empfängt Bytes.
*   **time**: Liefert blockierende Verzögerungen (`delay_ms`) und Zeitzählung (`millis`) über Timer.

### 3. `src/display/`
Enthält die Logik zur Ansteuerung des E-Ink-Displays.
*   **epd**: Implementiert das SPI-Protokoll (Software oder Hardware), um Befehle und Daten an den Controller zu senden. Verwaltet die Einschaltsequenz, ggf. Look-Up-Tables (LUTs) und den Refresh.

### 4. `src/rf/`
Verwaltet das 2,4-GHz-Radio.
*   **rf**: Konfiguriert die Radio-Register (Frequenz, Leistung, Modulation), steuert den DMA für schnelle Pakettransfers und die Zustandsmaschine des Radios (RX/TX/IDLE).

### 5. `src/flash/`
Treiber für die externe Flash.
*   **flash**: Implementiert Standard-SPI-Befehle (JEDEC) zum Lesen der ID, Lesen von Daten, Beschreiben von Seiten und Löschen von Sektoren der Winbond-Flash.

### 6. `src/cobs/`
Implementiert den Algorithmus **Consistent Overhead Byte Stuffing**.
*   **cobs**: Verpackt Daten für die serielle Übertragung und grenzt Pakete mit einem `0x00`-Byte ab, ohne dass dieses Byte in den Nutzdaten auftauchen kann — das erleichtert Synchronisation und Fehlererholung.

### 7. `Makefile`
Das von `make` genutzte Build-Skript. Definiert:
*   den Compiler (`sdcc`),
*   die zu kompilierenden Quelldateien,
*   die Compiler-Flags,
*   die Regeln zur Erzeugung der finalen `.hex`-Datei.
