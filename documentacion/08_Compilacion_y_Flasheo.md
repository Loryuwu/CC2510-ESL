# 08. Kompilierung und Flashen

Anleitung zum Bauen der Firmware und zum Programmieren des CC2510.

## Voraussetzungen

### Software
1.  **SDCC (Small Device C Compiler):** optimierter C-Compiler für 8-Bit-Mikrocontroller (8051).
2.  **Make:** Build-Automatisierung (GNU Make).
3.  **SmartRF Flash Programmer (v1):** Software von Texas Instruments zum Programmieren des Chips (Windows).

### Hardware
1.  **CC-Debugger:** offizieller Programmierer/Debugger von TI für die CCxxxx-Familie.
2.  **Verbindungskabel:** An die Programmier-Pads des ESL-Tags müssen Kabel gelötet werden.
    *   **GND**
    *   **VCC** (3,3 V)
    *   **DC** (Debug Clock – P2_2)
    *   **DD** (Debug Data – P2_1)
    *   **RST** (Reset – P2_0)

## Kompilierung

Das Projekt enthält ein `Makefile`, das den Vorgang vereinfacht (es erkennt Linux und Windows automatisch).

### Befehle
Ein Terminal im Projektverzeichnis öffnen und ausführen:

1.  **Alles bauen:**
    ```bash
    make
    ```
    Erzeugt bei fehlerfreiem Lauf die Datei `firmware.hex`.

2.  **Temporäre Dateien aufräumen:**
    ```bash
    make clean
    ```
    Entfernt die beim Kompilieren erzeugten `.rel`-, `.asm`-, `.lst`-, `.sym`-Dateien usw.

## Flashen

1.  Den **CC-Debugger** mit PC und ESL-Tag verbinden (die LED am Debugger sollte grün leuchten).
2.  **SmartRF Flash Programmer** öffnen.
3.  Im oberen Reiter „System-on-Chip" wählen.
4.  Das Gerät „CC2510" sollte in der Liste erscheinen.
5.  Unter „Flash image" die erzeugte Datei `firmware.hex` auswählen.
6.  Unter „Actions" „Erase, program and verify" anhaken.
7.  Auf „Perform actions" klicken.

Ist alles korrekt, läuft die Fortschrittsleiste durch und die Firmware startet auf dem Tag.
