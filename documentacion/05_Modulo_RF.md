# 05. RF-Modul (Funk)

Der CC2510 integriert einen 2,4-GHz-RF-Transceiver (ähnlich dem CC2500). Das Modul `src/rf/rf.c` verwaltet Konfiguration und Datenübertragung.

## Konfigurierte Eigenschaften
*   **Frequenz:** 2433 MHz (über die Register `FREQ2`, `FREQ1`, `FREQ0` einstellbar).
*   **Modulation:** MSK (Minimum Shift Keying).
*   **Datenrate:** 250 kBaud.
*   **Leistung:** 0 dBm (einstellbar in `PA_TABLE0`).
*   **Pakete:** variable Länge, mit aktiviertem CRC.

## Treiberarchitektur

### 1. DMA (Direct Memory Access)
Der Treiber nutzt den DMA-Controller des CC2510, um Daten automatisch zwischen RAM und dem Radio-Puffer (RFD) zu bewegen. Das entlastet die CPU und ermöglicht schnelle Transfers.
*   **DMA-Kanal 0:** dynamisch für TX oder RX konfiguriert.
*   **TX:** RAM → RFD.
*   **RX:** RFD → RAM.

### 2. Zustandsmaschine (`RFST`)
Das Radio wird über Strobe-Befehle an das Register `RFST` gesteuert:
*   `RFST_SIDLE` (0x04): Radio in den Leerlauf (Idle) versetzen.
*   `RFST_STX` (0x03): Senden starten.
*   `RFST_SRX` (0x02): Empfangen starten.

### 3. Haupt-API

#### `rf_init()`
Konfiguriert alle Radio-Register (Modem, Frequenz, AGC usw.) mit vorberechneten Werten (üblicherweise aus SmartRF Studio). Richtet den DMA ein und lässt das Radio standardmäßig im RX-Modus.

#### `rf_send_packet(uint8_t *data, uint8_t len)`
1.  Bereitet einen internen Puffer im Format `[Länge, Daten...]` vor.
2.  Versetzt das Radio in IDLE.
3.  Konfiguriert den DMA für den Transfer vom Puffer ins Register `X_RFD`.
4.  Aktiviert den DMA.
5.  Sendet den Befehl `STX`. Das Radio fordert die Daten während des Sendens über DMA an.
6.  Wartet auf das Ende der Übertragung (Interrupt `RFIF_IRQ_DONE`).
7.  Kehrt in den RX-Modus zurück.

#### `rf_receive_packet(uint8_t *buffer, uint8_t max_len)`
Prüft, ob ein vollständiges Paket empfangen wurde.
1.  Prüft das Interrupt-Flag `RFIF_IRQ_DONE` bzw. den DMA-Status.
2.  Liest bei vorhandenen Daten die Länge aus dem ersten Byte.
3.  Kopiert die Daten vom internen Puffer in den Benutzerpuffer.
4.  Scharfschalten des RX-Modus für das nächste Paket.

## Hinweise
*   Die Kalibrierung des Frequenzsynthesizers (`FSCAL`) erfolgt automatisch beim Übergang von IDLE zu TX/RX (konfiguriert in `MCSM0`).
*   Der Quarzoszillator (XOSC) muss stabil sein, bevor das Radio genutzt wird.
