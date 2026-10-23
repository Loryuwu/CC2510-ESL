/*
 * nRF24L01+ Channel Scanner
 * Sweeps all 126 channels (2400-2525 MHz) and shows signal activity.
 * Use this to find the CC2510's actual transmit frequency.
 *
 * ESP32-S3 Zero wiring: same as gateway_rx_test
 */

#include <SPI.h>
#include <RF24.h>

#define CE_PIN   1
#define CSN_PIN  2

RF24 radio(CE_PIN, CSN_PIN);

#define NUM_CHANNELS 126
uint16_t hits[NUM_CHANNELS];

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println();
    Serial.println("=== nRF24 Channel Scanner ===");
    Serial.println("Scanning 2400-2525 MHz for CC2510 signal");
    Serial.println("Make sure CC2510 is transmitting!");
    Serial.println();

    SPI.begin(3, 9, 7, CSN_PIN);

    if (!radio.begin(&SPI)) {
        Serial.println("ERROR: nRF24 not found!");
        while (1) delay(1000);
    }

    radio.setAutoAck(false);
    radio.setDataRate(RF24_250KBPS);
    radio.disableCRC();
    radio.stopListening();

    memset(hits, 0, sizeof(hits));

    Serial.println("Scanning... (takes ~10 seconds per sweep)");
    Serial.println();
}

void loop() {
    /* Sweep all channels, dwell on each for ~200us */
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        radio.setChannel(ch);
        radio.startListening();
        delayMicroseconds(250);  /* dwell time */
        radio.stopListening();

        if (radio.testRPD()) {
            hits[ch]++;
        }
    }

    /* Print results */
    Serial.println("Channel activity (2400 + ch MHz):");
    Serial.println("  ch  freq   hits");

    bool any = false;
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        if (hits[ch] > 0) {
            Serial.printf("  %3d  %4d   %d\n", ch, 2400 + ch, hits[ch]);
            any = true;
        }
    }

    if (!any) {
        Serial.println("  (no signals detected)");
    }

    /* Also print a visual bar chart for channels 60-90 (where we expect CC2510) */
    Serial.println();
    Serial.println("Detail ch 60-100:");
    for (int ch = 60; ch <= 100; ch++) {
        Serial.printf("  %3d %4dMHz |", ch, 2400 + ch);
        for (int b = 0; b < hits[ch] && b < 50; b++) Serial.print('#');
        Serial.println();
    }

    Serial.println();
    Serial.println("--- sweep done, repeating... ---");
    Serial.println();

    /* Reset for next sweep */
    memset(hits, 0, sizeof(hits));
    delay(1000);
}
