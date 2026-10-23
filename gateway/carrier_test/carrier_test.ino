/*
 * Full diagnostic: WiFi self-test + extended scan + ch76 listener
 */

#include <SPI.h>
#include <RF24.h>

#define CE_PIN   1
#define CSN_PIN  2

RF24 radio(CE_PIN, CSN_PIN);

uint8_t readReg(uint8_t reg) {
    uint8_t val;
    digitalWrite(CSN_PIN, LOW);
    SPI.transfer(reg & 0x1F);
    val = SPI.transfer(0xFF);
    digitalWrite(CSN_PIN, HIGH);
    return val;
}

void writeReg(uint8_t reg, uint8_t val) {
    digitalWrite(CSN_PIN, LOW);
    SPI.transfer(0x20 | (reg & 0x1F));
    SPI.transfer(val);
    digitalWrite(CSN_PIN, HIGH);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println();
    Serial.println("=== Full Diagnostic ===");

    SPI.begin(3, 9, 7, CSN_PIN);

    if (!radio.begin(&SPI)) {
        Serial.println("ERROR: nRF24 not found!");
        while (1) delay(1000);
    }

    radio.setAutoAck(false);
    radio.setPALevel(RF24_PA_MAX);
    radio.disableCRC();
    radio.setPayloadSize(32);

    /* === STEP 1: WiFi self-test (1 Mbps, scan ch 1-13) === */
    Serial.println("Step 1: WiFi detection (verifying nRF24 works)...");
    radio.setDataRate(RF24_1MBPS);
    uint16_t wifiHits = 0;
    for (int sweep = 0; sweep < 50; sweep++) {
        for (int ch = 1; ch <= 13; ch++) {
            radio.setChannel(ch);
            radio.startListening();
            delayMicroseconds(200);
            radio.stopListening();
            if (radio.testRPD()) wifiHits++;
        }
    }
    Serial.printf("WiFi hits: %d (expect >0 if WiFi nearby)\n", wifiHits);
    if (wifiHits > 0) Serial.println("nRF24 RX: WORKING");
    else Serial.println("nRF24 RX: WARNING - no WiFi detected (module issue?)");

    /* === STEP 2: Extended channel scan at 250kbps === */
    Serial.println();
    Serial.println("Step 2: Extended scan (250kbps, 20 sweeps, 1ms dwell)...");
    radio.setDataRate(RF24_250KBPS);

    uint16_t hits[126];
    memset(hits, 0, sizeof(hits));

    for (int sweep = 0; sweep < 20; sweep++) {
        for (int ch = 0; ch < 126; ch++) {
            radio.setChannel(ch);
            radio.startListening();
            delayMicroseconds(1000);
            radio.stopListening();
            if (radio.testRPD()) hits[ch]++;
        }
    }

    Serial.println("Channels with signal:");
    bool found = false;
    for (int ch = 0; ch < 126; ch++) {
        if (hits[ch] > 0) {
            Serial.printf("  ch%d (%dMHz): %d/20\n", ch, 2400+ch, hits[ch]);
            found = true;
        }
    }
    if (!found) Serial.println("  NONE");

    /* Detail around ch76 */
    Serial.println("Detail ch70-82:");
    for (int ch = 70; ch <= 82; ch++) {
        Serial.printf("  ch%d: %d", ch, hits[ch]);
        if (ch == 76) Serial.print(" <-- expected");
        Serial.println();
    }

    /* === STEP 3: Continuous listener on ch76 === */
    Serial.println();
    Serial.println("Step 3: Listening on ch76 (addr A7:5C:A7)...");

    radio.setChannel(76);
    radio.setAddressWidth(3);
    uint8_t addr[] = {0xA7, 0x5C, 0xA7};
    radio.openReadingPipe(0, addr);
    radio.startListening();

    uint8_t cfg = readReg(0x00);
    cfg &= ~0x0C;
    writeReg(0x00, cfg);

    /* Force and verify the exact hardware receive configuration. */
    writeReg(0x03, 0x01);  /* SETUP_AW: 01 = 3-byte address */
    writeReg(0x02, 0x01);  /* EN_RXADDR: enable only pipe 0 */
    writeReg(0x11, 32);    /* RX_PW_P0: fixed 32-byte payload */

    Serial.printf("CONFIG: 0x%02X\n", readReg(0x00));
    Serial.printf("SETUP_AW: 0x%02X (expect 0x01 = 3 bytes)\n", readReg(0x03));
    Serial.printf("EN_RXADDR: 0x%02X (expect 0x01 = pipe 0 only)\n", readReg(0x02));
    Serial.printf("RX_PW_P0: %d (expect 32)\n", readReg(0x11));
    Serial.println("Monitoring RPD and packets...");
    Serial.println();
}

uint32_t pktCount = 0;
uint32_t rpdTotal = 0;

void loop() {
    if (radio.available()) {
        uint8_t buf[32];
        radio.read(buf, 32);
        pktCount++;

        Serial.printf("[%lu] PKT #%lu: ", millis(), pktCount);
        for (int i = 0; i < 16; i++) Serial.printf("%02X ", buf[i]);
        Serial.println("...");
    }

    static uint32_t lastStatus = 0;
    if (millis() - lastStatus > 5000) {
        lastStatus = millis();
        bool rpd = radio.testRPD();
        if (rpd) rpdTotal++;
        Serial.printf("[%lu] pkts=%lu rpd=%d rpd_total=%lu\n",
                      millis(), pktCount, rpd, rpdTotal);
    }
}
