/*
 * PriceTag Gateway - Radio Loopback Diagnostic
 *
 * Partner sketch for firmware/src/main_radio_loopback.c.
 *
 * Makes the ESP->CC direction VISIBLE. This sketch ping-pongs:
 *   1. TX phase: send ONE test packet to the CC2510 (5-byte address, the
 *      AA AA prefix acts as extra preamble for the CC2510 demodulator).
 *   2. RX phase: switch to RX and listen briefly for the CC2510's echo
 *      (3-byte address A7 5C A7, CRC off - the proven CC->ESP path).
 *
 * The CC2510 is always listening and echoes back whatever its receiver
 * captured. What you see on serial:
 *
 *   HB  seq=N                 -> heartbeat. CC is alive and CC->ESP works,
 *                                but the CC RX did NOT lock onto our packet.
 *                                If you ONLY ever see HB -> PHY problem
 *                                (preamble/timing/modulation), not decode.
 *
 *   ECHO got=M  raw: A7 01 .. -> the CC2510 locked sync and captured M raw
 *                                bytes. These are the GROUND TRUTH of what
 *                                the CC receiver sees. Expect raw[0]=A7
 *                                (nRF24 addr byte 3), raw[1]=01 (tag addr),
 *                                raw[2]=len, then "PING"+seq. The alignment
 *                                here tells us the correct offset/shift for
 *                                radio_rx().
 *
 * Wiring (ESP32-S3 Zero -> nRF24L01+): same as gateway_rx_test.
 *   3.3V->VCC  GND->GND  GPIO1->CE  GPIO2->CSN  GPIO3->SCK  GPIO7->MOSI  GPIO9->MISO
 */

#include <SPI.h>
#include <RF24.h>

#define CE_PIN   1
#define CSN_PIN  2

#define NRF_CHANNEL      76
#define NRF_PAYLOAD_SIZE 32

/* TX to CC: the nRF24 transmits multi-byte addresses REVERSED on air
 * (MSByte = array[last] first). So to get on-air AA AA A7 5C A7 (AA AA as
 * extended preamble BEFORE the CC sync word A7 5C), the array must be the
 * reverse. Verified via loopback: the old {AA,AA,A7,5C,A7} put the AA AA
 * AFTER the sync (capture "A7 AA AA 01.."); this order gives "A7 01 .." . */
const uint8_t txAddr[5] = {0xA7, 0x5C, 0xA7, 0xAA, 0xAA};
/* RX from CC: 3-byte A7 5C A7 (CC sends sync A7 5C, then A7 as data). */
const uint8_t rxAddr[3] = {0xA7, 0x5C, 0xA7};

RF24 radio(CE_PIN, CSN_PIN);

uint16_t seq = 0;

uint8_t readReg(uint8_t reg) {
    uint8_t v;
    digitalWrite(CSN_PIN, LOW);
    SPI.transfer(reg & 0x1F);
    v = SPI.transfer(0xFF);
    digitalWrite(CSN_PIN, HIGH);
    return v;
}

void writeReg(uint8_t reg, uint8_t val) {
    digitalWrite(CSN_PIN, LOW);
    SPI.transfer(0x20 | (reg & 0x1F));
    SPI.transfer(val);
    digitalWrite(CSN_PIN, HIGH);
}

void forceCrcOff() {
    /* RF24 lib re-enables CRC in startListening(); clear it directly. */
    uint8_t cfg = readReg(0x00);
    cfg &= ~0x0C;                 /* clear EN_CRC (b3) + CRCO (b2) */
    writeReg(0x00, cfg);
}

void switchToTx() {
    radio.stopListening();
    radio.setAddressWidth(5);
    radio.openWritingPipe(txAddr);
}

void switchToRx() {
    radio.setAddressWidth(3);
    radio.openReadingPipe(0, rxAddr);
    radio.startListening();
    forceCrcOff();
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println();
    Serial.println("=== PriceTag Gateway Loopback Diagnostic ===");

    SPI.begin(3, 9, 7, CSN_PIN);

    if (!radio.begin(&SPI)) {
        Serial.println("ERROR: nRF24 not found! Check wiring.");
        while (1) delay(1000);
    }

    radio.setChannel(NRF_CHANNEL);
    radio.setDataRate(RF24_250KBPS);
    radio.setPALevel(RF24_PA_MAX);
    radio.setPayloadSize(NRF_PAYLOAD_SIZE);
    radio.setAutoAck(false);
    radio.setRetries(0, 0);
    radio.disableDynamicPayloads();
    radio.disableCRC();

    Serial.println("ch=76, 250kbps, CRC off, AutoAck off");
    Serial.println("TX addr AA:AA:A7:5C:A7 | RX addr A7:5C:A7");
    Serial.println("Ping-ponging... (HB = heartbeat, ECHO = CC captured bytes)");
    Serial.println();
}

void printRx() {
    uint8_t raw[NRF_PAYLOAD_SIZE];
    radio.read(raw, sizeof(raw));

    /* CC wraps payload as [0x01=addr][len][payload...][xor]; payload[0] is
     * our marker (0xBB heartbeat / 0xE0 echo). */
    uint8_t len    = raw[1];
    uint8_t marker = raw[2];

    if (marker == 0xBB) {
        uint16_t s = ((uint16_t)raw[3] << 8) | raw[4];
        Serial.printf("[%lu] HB   seq=%u\n", millis(), s);
    } else if (marker == 0xE0) {
        /* payload: [0xE0][got][marc][pktst][raw...] -> at raw[2..] */
        uint8_t got   = raw[3];
        uint8_t marc  = raw[4];
        uint8_t pktst = raw[5];
        const char *why = "?";
        switch (marc) {
            case 0x11: why = "RXOVF (FIFO overflow)"; break;
            case 0x0D: why = "still RX (demod lost bit-sync)"; break;
            case 0x01: why = "IDLE (pkt engine ended early)"; break;
        }
        Serial.printf("[%lu] ECHO got=%u marc=0x%02X(%s) pktst=0x%02X  raw:",
                      millis(), got, marc, why, pktst);
        for (uint8_t i = 0; i < got && (6 + i) < NRF_PAYLOAD_SIZE; i++)
            Serial.printf(" %02X", raw[6 + i]);
        Serial.println();
    } else {
        Serial.printf("[%lu] ???  marker=0x%02X len=%u  full:", millis(), marker, len);
        for (uint8_t i = 0; i < 12; i++) Serial.printf(" %02X", raw[i]);
        Serial.println();
    }
}

void loop() {
    /* --- TX phase: one recognizable test packet to the CC --- */
    switchToTx();

    uint8_t payload[NRF_PAYLOAD_SIZE];
    memset(payload, 0, sizeof(payload));
    uint8_t data[] = {'P', 'I', 'N', 'G',
                      (uint8_t)(seq >> 8), (uint8_t)(seq & 0xFF)};
    uint8_t dlen = sizeof(data);
    payload[0] = 0x01;            /* tag addr */
    payload[1] = dlen;
    memcpy(&payload[2], data, dlen);
    uint8_t x = 0;
    for (uint8_t i = 0; i < dlen + 2; i++) x ^= payload[i];
    payload[dlen + 2] = x;

    radio.write(payload, sizeof(payload));
    seq++;

    /* --- RX phase: listen for the CC's echo/heartbeat --- */
    switchToRx();

    uint32_t t0 = millis();
    while (millis() - t0 < 120) {
        if (radio.available()) printRx();
    }
}
