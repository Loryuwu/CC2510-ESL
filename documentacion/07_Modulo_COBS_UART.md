# 07. COBS-UART-Modul

Dieses Modul (`src/cobs/cobs.c`) implementiert eine robuste Transportschicht über der seriellen UART-Verbindung.

## Was ist COBS?
**Consistent Overhead Byte Stuffing (COBS)** ist ein Algorithmus, der Datenpakete variabler Länge so kodiert, dass das Byte `0x00` niemals im Nachrichtenkörper vorkommt. Dadurch lässt sich `0x00` als eindeutiger Paketende-Begrenzer verwenden.

### Vorteile
*   **Synchronisation:** Verliert der Empfänger Bytes oder steigt mitten in eine Übertragung ein, kann er sich leicht am nächsten `0x00`-Byte resynchronisieren.
*   **Effizienz:** fügt nur minimalen Overhead hinzu (ca. 1 Byte pro 254 Datenbytes).

## Implementierung

### Zustandsstruktur (`CobsState`)
Hält den Zustand der laufenden Dekodierung:
```c
typedef struct {
  uint8_t packet[MAX_PACKET_SIZE]; // Puffer für das dekodierte Paket
  uint8_t packet_size;             // aktuelle Paketgröße
  uint8_t packet_index;            // Schreibindex
  uint8_t block, code;             // interne Variablen des Algorithmus
} CobsState;
```

### Hauptfunktionen

#### `cobs_init(CobsState *state)`
Setzt die Zustandsmaschine zurück, um ein neues Paket zu empfangen.

#### `cobs_handle(CobsState *state)`
Muss periodisch in der Hauptschleife aufgerufen werden.
1.  Versucht, ein Byte von der UART zu lesen (`uart_read_byte`).
2.  Verarbeitet das Byte nach dem COBS-Algorithmus.
3.  Ist das empfangene Byte `0x00` und das Paket gültig, gibt es `true` zurück — ein vollständiges Paket liegt in `state->packet`.

#### `cobs_send(const uint8_t *data, uint8_t size)`
Kodiert einen Datenpuffer und sendet ihn.
1.  Durchläuft die Daten und berechnet die COBS-Blöcke und -Codes.
2.  Sendet die kodierten Bytes über UART.
3.  Schließt mit einem `0x00`-Byte als Begrenzer ab.

## Verwendung im Projekt
Das System nutzt COBS für:
1.  **Debugging:** Textnachrichten senden (`cobs_send_str`), die am PC angezeigt werden können.
2.  **Befehle:** komplexe Anweisungen vom PC empfangen (z. B. „Bild aktualisieren", „RF konfigurieren") ohne Gefahr der Desynchronisation.

### Testskript (`cobs_test.py`)
Enthalten ist ein Python-Skript, das mit den Bibliotheken `cobs` und `pyserial` vom PC aus mit der Firmware kommuniziert und so das Senden und Empfangen kodierter Pakete demonstriert.
