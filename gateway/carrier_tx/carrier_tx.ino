/*
 * PriceTag Gateway - nRF24L01+ CONSTANT CARRIER TX
 *
 * Emits an unmodulated continuous carrier on channel 76 (2476 MHz).
 * This is the most unambiguous signal possible - no framing, no bursts.
 * Use with the CC2510 RSSI meter firmware: if the CC2510's green blink
 * count jumps up while this runs, the nRF24 transmission reaches it.
 *
 * Toggles PA level every 3 seconds (MAX / HIGH / LOW / MIN) and prints
 * which level is active, so you can see if a lower PA (less current,
 * no brownout) actually transmits while PA_MAX does not.
 *
 * Same wiring as the other gateway sketches.
 */

#include <SPI.h>
#include <RF24.h>

#define CE_PIN   1
#define CSN_PIN  2
#define NRF_CHANNEL 76

RF24 radio(CE_PIN, CSN_PIN);

const rf24_pa_dbm_e levels[4] = { RF24_PA_MAX, RF24_PA_HIGH, RF24_PA_LOW, RF24_PA_MIN };
const char *names[4] = { "MAX", "HIGH", "LOW", "MIN" };
uint8_t idx = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println();
    Serial.println("=== nRF24 CONSTANT CARRIER on ch76 ===");

    SPI.begin(3, 9, 7, CSN_PIN);

    if (!radio.begin(&SPI)) {
        Serial.println("ERROR: nRF24 not found! Check wiring.");
        while (1) delay(1000);
    }

    radio.setDataRate(RF24_250KBPS);
    Serial.println("Cycling PA levels every 3s. Watch the CC2510 RSSI meter.");
}

void loop() {
    radio.stopConstCarrier();
    delay(50);
    radio.startConstCarrier(levels[idx], NRF_CHANNEL);
    Serial.printf("Constant carrier ON, PA=%s\n", names[idx]);
    idx = (idx + 1) & 3;
    delay(3000);
}
