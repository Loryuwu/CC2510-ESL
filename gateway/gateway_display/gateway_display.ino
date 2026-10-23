/*
 * PriceTag Gateway - Phase 1 Display Driver
 *
 * Partner sketch for firmware/src/main_display.c. Sends drawing commands to
 * the CC2510, which renders them on the e-ink. Demonstrates the end-to-end
 * ESP32 -> nRF24 -> CC2510 -> IL0373 path with a simulated Home Assistant
 * layout (weather / energy / status), updated on a counter so you can see it
 * refresh.
 *
 * Protocol (one command per packet): payload = [0x01][n][cmd+params][xor]
 *   CMD_CLEAR  0x01 [color]
 *   CMD_RECT   0x02 [x_hi][x_lo][y][w_hi][w_lo][h][color]
 *   CMD_TEXT   0x03 [x_hi][x_lo][y][scale][color][string]
 *   CMD_UPDATE 0x10            -> CC renders, then ACKs
 *   CMD_PING   0xF0            -> CC ACKs (handshake)
 * The CC replies with [0x01][2][0xF1][status][xor].
 *
 * Colors: 0=WHITE 1=BLACK 2=RED. Screen 296x152.
 *
 * Wiring (ESP32-S3 Zero -> nRF24L01+): same as gateway_loopback.
 *   3.3V->VCC GND->GND GPIO1->CE GPIO2->CSN GPIO3->SCK GPIO7->MOSI GPIO9->MISO
 */

#include <SPI.h>
#include <RF24.h>

#define CE_PIN   1
#define CSN_PIN  2

#define NRF_CHANNEL      76
#define NRF_PAYLOAD_SIZE 32

#define TAG_ADDR 0x01     /* target CC2510 address */

#define CMD_CLEAR  0x01
#define CMD_RECT   0x02
#define CMD_TEXT   0x03
#define CMD_UPDATE 0x10
#define CMD_PING   0xF0
#define CMD_ACK    0xF1

#define WHITE 0
#define BLACK 1
#define RED   2

/* nRF24 sends addresses reversed on air: array {A7,5C,A7,AA,AA} -> on-air
 * AA AA A7 5C A7 (AA AA = preamble, A7 5C = CC sync word). RX pipe for the
 * CC's ACK uses the palindrome A7 5C A7. */
const uint8_t txAddr[5] = {0xA7, 0x5C, 0xA7, 0xAA, 0xAA};
const uint8_t rxAddr[3] = {0xA7, 0x5C, 0xA7};

RF24 radio(CE_PIN, CSN_PIN);

uint16_t frameCounter = 0;

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
    uint8_t cfg = readReg(0x00);
    cfg &= ~0x0C;
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

/* Send one command packet: [TAG_ADDR][n][data...][xor]. */
void sendCmd(const uint8_t *data, uint8_t n) {
    uint8_t p[NRF_PAYLOAD_SIZE];
    memset(p, 0, sizeof(p));
    p[0] = TAG_ADDR;
    p[1] = n;
    memcpy(&p[2], data, n);
    uint8_t x = 0;
    for (uint8_t i = 0; i < n + 2; i++) x ^= p[i];
    p[n + 2] = x;

    switchToTx();
    radio.write(p, sizeof(p));
}

/* Wait for the CC's ACK (raw[2] == 0xF1). Returns true on success. */
bool waitAck(uint32_t timeout_ms) {
    switchToRx();
    uint32_t t0 = millis();
    while (millis() - t0 < timeout_ms) {
        if (radio.available()) {
            uint8_t raw[NRF_PAYLOAD_SIZE];
            radio.read(raw, sizeof(raw));
            if (raw[0] == TAG_ADDR && raw[2] == CMD_ACK)
                return true;
        }
    }
    return false;
}

void cmdClear(uint8_t color) {
    uint8_t d[2] = {CMD_CLEAR, color};
    sendCmd(d, 2);
    delay(40);
}
void cmdRect(uint16_t x, uint8_t y, uint16_t w, uint8_t h, uint8_t color) {
    uint8_t d[8] = {CMD_RECT, (uint8_t)(x >> 8), (uint8_t)x, y,
                    (uint8_t)(w >> 8), (uint8_t)w, h, color};
    sendCmd(d, 8);
    delay(40);
}
void cmdText(uint16_t x, uint8_t y, uint8_t scale, uint8_t color, const char *s) {
    uint8_t d[NRF_PAYLOAD_SIZE];
    uint8_t n = 0;
    d[n++] = CMD_TEXT;
    d[n++] = (uint8_t)(x >> 8);
    d[n++] = (uint8_t)x;
    d[n++] = y;
    d[n++] = scale;
    d[n++] = color;
    while (*s && n < 29) d[n++] = (uint8_t)(*s++);
    sendCmd(d, n);
    delay(40);
}

bool handshake() {
    for (uint8_t attempt = 0; attempt < 20; attempt++) {
        uint8_t d[1] = {CMD_PING};
        sendCmd(d, 1);
        if (waitAck(300)) {
            Serial.printf("[%lu] PING -> ACK (attempt %u)\n", millis(), attempt + 1);
            return true;
        }
    }
    Serial.println("PING: no ACK (CC not responding)");
    return false;
}

void sendFrame() {
    char line[24];

    cmdClear(WHITE);
    cmdRect(0, 0, 296, 24, RED);
    cmdText(70, 6, 2, WHITE, "HA DISPLAY");

    cmdText(10, 40, 2, BLACK, "Wetter: 18C Sonne");
    cmdText(10, 68, 2, BLACK, "PV: 2.4 kW");
    cmdText(10, 96, 2, BLACK, "Netz: OK");

    snprintf(line, sizeof(line), "Update #%u", frameCounter);
    cmdText(10, 128, 1, BLACK, line);

    /* UPDATE triggers the (slow) e-ink refresh; the CC ACKs when done. */
    uint8_t d[1] = {CMD_UPDATE};
    sendCmd(d, 1);
    Serial.printf("[%lu] frame #%u sent, waiting for refresh ACK...\n",
                  millis(), frameCounter);

    if (waitAck(25000))
        Serial.printf("[%lu] UPDATE -> ACK (display refreshed)\n", millis());
    else
        Serial.println("UPDATE: no ACK (refresh timeout?)");
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println();
    Serial.println("=== PriceTag Gateway Display Driver ===");

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

    Serial.println("ch=76, 250kbps, CRC off. Target tag 0x01.");

    handshake();
}

void loop() {
    sendFrame();
    frameCounter++;
    delay(10000);   /* new frame every 10 s */
}
