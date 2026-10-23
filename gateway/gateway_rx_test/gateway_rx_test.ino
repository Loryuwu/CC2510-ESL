/*
 * PriceTag Gateway - nRF24L01+ RX Test (Debug Version)
 *
 * Wiring (ESP32-S3 Zero -> nRF24L01+):
 *   3.3V -> VCC  (use 3.3V, NOT 5V!)
 *   GND  -> GND
 *   GPIO 1  -> CE
 *   GPIO 2  -> CSN
 *   GPIO 3  -> SCK
 *   GPIO 7  -> MOSI
 *   GPIO 9  -> MISO
 *
 * Install RF24 library: Arduino Library Manager -> "RF24" by TMRh20
 */

#include <SPI.h>
#include <RF24.h>

#define CE_PIN   1
#define CSN_PIN  2

#define NRF_CHANNEL    76
#define NRF_ADDR_WIDTH 3
#define NRF_PAYLOAD_SIZE 32

const uint8_t rxAddr[NRF_ADDR_WIDTH] = {0xA7, 0x5C, 0xA7};

RF24 radio(CE_PIN, CSN_PIN);

/* Read nRF24 register directly */
uint8_t readReg(uint8_t reg) {
    uint8_t val;
    digitalWrite(CSN_PIN, LOW);
    SPI.transfer(reg & 0x1F);
    val = SPI.transfer(0xFF);
    digitalWrite(CSN_PIN, HIGH);
    return val;
}

/* Write nRF24 register directly */
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
    Serial.println("=== PriceTag Gateway RX Test ===");

    SPI.begin(3, 9, 7, CSN_PIN);  /* SCK, MISO, MOSI, SS */

    if (!radio.begin(&SPI)) {
        Serial.println("ERROR: nRF24 not found! Check wiring.");
        while (1) delay(1000);
    }

    radio.setChannel(NRF_CHANNEL);
    radio.setDataRate(RF24_250KBPS);
    radio.setPALevel(RF24_PA_LOW);
    radio.setAddressWidth(NRF_ADDR_WIDTH);
    radio.setPayloadSize(NRF_PAYLOAD_SIZE);
    radio.setAutoAck(false);
    radio.setRetries(0, 0);
    radio.disableDynamicPayloads();
    radio.disableCRC();

    radio.openReadingPipe(0, rxAddr);
    radio.startListening();

    /*
     * CRITICAL FIX: RF24 library re-enables CRC in startListening()
     * because it uses a cached config_reg. Force CRC off again by
     * writing CONFIG register directly: keep PRIM_RX and PWR_UP,
     * clear EN_CRC and CRCO.
     */
    uint8_t cfg = readReg(0x00);  /* CONFIG register */
    Serial.printf("CONFIG before fix: 0x%02X (CRC %s)\n",
                  cfg, (cfg & 0x08) ? "ON!" : "off");

    cfg &= ~0x0C;  /* clear EN_CRC (bit3) and CRCO (bit2) */
    writeReg(0x00, cfg);

    cfg = readReg(0x00);
    Serial.printf("CONFIG after fix:  0x%02X (CRC %s)\n",
                  cfg, (cfg & 0x08) ? "ON!" : "off");

    /* Verify other settings */
    Serial.printf("EN_AA:     0x%02X (should be 0x00)\n", readReg(0x01));
    Serial.printf("EN_RXADDR: 0x%02X\n", readReg(0x02));
    Serial.printf("SETUP_AW:  0x%02X (01=3B, 02=4B, 03=5B)\n", readReg(0x03));
    Serial.printf("RF_CH:     %d (freq %d MHz)\n", readReg(0x05), 2400 + readReg(0x05));
    Serial.printf("RF_SETUP:  0x%02X\n", readReg(0x06));
    Serial.printf("RX_PW_P0:  %d\n", readReg(0x11));
    Serial.printf("DYNPD:     0x%02X\n", readReg(0x1C));
    Serial.printf("FEATURE:   0x%02X\n", readReg(0x1D));
    Serial.println();
    Serial.println("Listening... (carrier detect active)");
    Serial.println();
}

uint32_t lastCarrierCheck = 0;
uint32_t carrierCount = 0;
uint32_t loopCount = 0;

void loop() {
    loopCount++;

    /* Check for received packets */
    if (radio.available()) {
        uint8_t raw[NRF_PAYLOAD_SIZE];
        radio.read(raw, sizeof(raw));

        Serial.printf("[%lu] RX PACKET! Raw hex:\n  ", millis());
        for (int i = 0; i < NRF_PAYLOAD_SIZE; i++) {
            Serial.printf("%02X ", raw[i]);
            if ((i + 1) % 16 == 0) Serial.print("\n  ");
        }
        Serial.println();

        /* Try to parse protocol */
        uint8_t tag_addr = raw[0];
        uint8_t pkt_len  = raw[1];

        if (pkt_len <= 32 && pkt_len + 3 <= NRF_PAYLOAD_SIZE) {
            uint8_t xor_val = 0;
            for (uint8_t i = 0; i < pkt_len + 2; i++)
                xor_val ^= raw[i];

            if (xor_val == raw[pkt_len + 2]) {
                Serial.printf("  -> VALID pkt from tag 0x%02X, len=%d\n", tag_addr, pkt_len);
                if (pkt_len >= 8) {
                    uint16_t seq = ((uint16_t)raw[2] << 8) | raw[3];
                    uint16_t vdd = ((uint16_t)raw[4] << 8) | raw[5];
                    Serial.printf("  -> seq=%u vdd=%umV msg=%c%c%c%c\n",
                                  seq, vdd, raw[6], raw[7], raw[8], raw[9]);
                }
            } else {
                Serial.printf("  -> XOR mismatch (got 0x%02X, expect 0x%02X)\n",
                              xor_val, raw[pkt_len + 2]);
            }
        } else {
            Serial.printf("  -> Can't parse (addr=0x%02X len=%d)\n", tag_addr, pkt_len);
        }
        Serial.println();
    }

    /* Carrier detect every 500ms */
    if (millis() - lastCarrierCheck > 500) {
        lastCarrierCheck = millis();

        /* RPD = Received Power Detector, set when signal > -64dBm */
        bool rpd = radio.testRPD();

        if (rpd) {
            carrierCount++;
            Serial.printf("[%lu] CARRIER DETECTED! (count=%lu)\n",
                          millis(), carrierCount);
        }

        /* Print status every 10 seconds */
        static uint32_t lastStatus = 0;
        if (millis() - lastStatus > 10000) {
            lastStatus = millis();
            Serial.printf("[%lu] Status: loops=%lu carriers=%lu\n",
                          millis(), loopCount, carrierCount);
            loopCount = 0;
            carrierCount = 0;
        }
    }
}
