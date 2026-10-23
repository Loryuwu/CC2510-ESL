# 06. Externes Flash-Modul

Dieses Modul (`src/flash/flash.c`) ermöglicht Lesen und Schreiben der externen SPI-Flash (Winbond W25X10CL).

## Hardware-Schnittstelle
Der Speicher teilt sich den SPI-Bus mit den RGB-LEDs, was eine sorgfältige Pin-Verwaltung erfordert.
*   **CS (Chip Select):** P1_4 (Ausgang)
*   **CLK (Clock):** P1_5 (SPI-Peripherie / rote LED)
*   **MOSI (Data In):** P1_6 (SPI-Peripherie / blaue LED)
*   **MISO (Data Out):** P1_7 (SPI-Peripherie / grüne LED)

> **Konflikt:** Sobald die SPI-Peripherie (USART1 Alt 2) aktiv ist, funktionieren P1_5, P1_6 und P1_7 nicht mehr als GPIO für die LEDs.

## SPI-Implementierung
Der Treiber nutzt **USART1** im Modus **SPI Master Alt 2**.
*   **Konfiguration:** `PERCFG |= 0x02` (U1CFG Alt 2).
*   **Transferfunktion:** `spi_transfer(uint8_t data)` schreibt ein Byte in `U1DBUF` und wartet auf den Abschluss des Austauschs.

## Unterstützte Befehle
Der Treiber implementiert eine Teilmenge der Standard-JEDEC-Befehle:

1.  **ID lesen (`spi_flash_read_id`)**
    *   Sendet `0x90` (Manufacturer ID).
    *   Liefert Hersteller- und Geräte-ID (Winbond = 0xEF). Nützlich zur Verbindungsprüfung.

2.  **Daten lesen (`spi_flash_read`)**
    *   Befehl `0x03`.
    *   Liest `len` Bytes ab einer 24-Bit-Adresse.

3.  **Schreiben (`spi_flash_write`)**
    *   Aktiviert Schreiben (`0x06` Write Enable).
    *   Nutzt „Page Program" (`0x02`) zum Schreiben.
    *   **Hinweis:** Flash-Speicher setzen voraus, dass der Bereich vor dem Schreiben gelöscht (0xFF) ist.

4.  **Löschen (`spi_flash_sector_erase`, `spi_flash_chip_erase`)**
    *   Sector Erase (`0x20`): löscht einen 4-KB-Sektor.
    *   Chip Erase (`0xC7`): löscht den gesamten Speicher.
    *   Diese Operationen dauern; der Treiber prüft das Statusregister (Bit WIP) und wartet auf das Ende (`spi_flash_wait_busy`).

5.  **Power Down (`spi_flash_power_down`)**
    *   Versetzt den Speicher in den Ultra-Niedrigverbrauchsmodus (`0xB9`).
    *   Muss vor erneuter Nutzung aufgeweckt werden (`0xAB`).
