/*
 * PriceTag Gateway - integrated (enrollment registry + display driver)
 *
 * Partner for firmware/src/main_display.c. Combines Milestone C (enroll +
 * NVS registry + serial approval) with driving each enrolled display: sends
 * a demo Home-Assistant layout (weather / energy / status) addressed to each
 * display's assigned address, personalised with its label id.
 *
 * Serial commands:  y = approve oldest pending, n = reject, l = list
 *
 * A frame is sent to each online display right after it comes online and
 * then every FRAME_INTERVAL ms. 3-colour e-ink refresh is slow (~15s) and
 * shouldn't be driven too often.
 *
 * Wiring: same as gateway_loopback.
 */

#include <SPI.h>
#include <RF24.h>
#include <Preferences.h>

#define CE_PIN   1
#define CSN_PIN  2
#define NRF_CHANNEL      76
#define NRF_PAYLOAD_SIZE 32

#define FRAME_INTERVAL 60000UL   /* ms between frames per display */

#define ENROLL_ADDR        0x00
#define CMD_CLEAR          0x01
#define CMD_RECT           0x02
#define CMD_TEXT           0x03
#define CMD_UPDATE         0x10
#define CMD_ACK            0xF1
#define CMD_ENROLL_REQ     0xE0
#define CMD_ENROLL_ASSIGN  0xE1
#define CMD_ENROLL_PENDING 0xE2

#define WHITE 0
#define BLACK 1
#define RED   2

const uint8_t txAddr[5] = {0xA7, 0x5C, 0xA7, 0xAA, 0xAA};
const uint8_t rxAddr[3] = {0xA7, 0x5C, 0xA7};

RF24 radio(CE_PIN, CSN_PIN);
Preferences prefs;

#define MAX_ENTRIES 16
struct Entry { uint8_t uid[7]; uint8_t addr; uint8_t label[4]; };
Entry    reg[MAX_ENTRIES];
uint8_t  regCount = 0;
bool     online[MAX_ENTRIES];
uint32_t lastFrame[MAX_ENTRIES];

#define MAX_PENDING 8
struct Pend { uint8_t uid[7]; uint8_t label[4]; };
Pend     pend[MAX_PENDING];
uint8_t  pendCount = 0;

/* ---------- helpers ---------- */
String hexBytes(const uint8_t *p, uint8_t n, char sep) {
    String s;
    for (uint8_t i = 0; i < n; i++) {
        if (i && sep) s += sep;
        if (p[i] < 16) s += '0';
        s += String(p[i], HEX);
    }
    s.toUpperCase();
    return s;
}
bool uidEq(const uint8_t *a, const uint8_t *b) {
    for (uint8_t i = 0; i < 7; i++) if (a[i] != b[i]) return false;
    return true;
}
int findReg(const uint8_t *uid) {
    for (uint8_t i = 0; i < regCount; i++) if (uidEq(reg[i].uid, uid)) return i;
    return -1;
}
int findPend(const uint8_t *uid) {
    for (uint8_t i = 0; i < pendCount; i++) if (uidEq(pend[i].uid, uid)) return i;
    return -1;
}
uint8_t nextFreeAddr() {
    for (uint16_t a = 1; a <= 254; a++) {
        bool used = false;
        for (uint8_t i = 0; i < regCount; i++) if (reg[i].addr == a) { used = true; break; }
        if (!used) return (uint8_t)a;
    }
    return 0;
}
void saveReg() { prefs.putBytes("reg", reg, regCount * sizeof(Entry)); }
void loadReg() {
    size_t n = prefs.getBytes("reg", reg, sizeof(reg));
    regCount = n / sizeof(Entry);
    for (uint8_t i = 0; i < MAX_ENTRIES; i++) { online[i] = false; lastFrame[i] = 0; }
}

/* ---------- nRF24 ---------- */
uint8_t readReg(uint8_t r){ uint8_t v; digitalWrite(CSN_PIN,LOW); SPI.transfer(r&0x1F); v=SPI.transfer(0xFF); digitalWrite(CSN_PIN,HIGH); return v; }
void writeReg(uint8_t r,uint8_t v){ digitalWrite(CSN_PIN,LOW); SPI.transfer(0x20|(r&0x1F)); SPI.transfer(v); digitalWrite(CSN_PIN,HIGH); }
void forceCrcOff(){ writeReg(0x00, readReg(0x00) & ~0x0C); }
void modeTx(){ radio.stopListening(); radio.setAddressWidth(5); radio.openWritingPipe(txAddr); }
void modeRx(){ radio.setAddressWidth(3); radio.openReadingPipe(0, rxAddr); radio.startListening(); forceCrcOff(); }

/* Low-level: build [dest][n][data][xor] and write (assumes TX mode). */
void writeCmd(uint8_t dest, const uint8_t *data, uint8_t n) {
    uint8_t p[NRF_PAYLOAD_SIZE];
    memset(p, 0, sizeof(p));
    p[0] = dest; p[1] = n;
    memcpy(&p[2], data, n);
    uint8_t x = 0; for (uint8_t i = 0; i < n + 2; i++) x ^= p[i];
    p[n + 2] = x;
    radio.write(p, sizeof(p));
}
/* Self-contained single command (switches TX then back to RX). */
void sendCmd(uint8_t dest, const uint8_t *data, uint8_t n) {
    modeTx(); writeCmd(dest, data, n); modeRx();
}

void replyAssign(const uint8_t *uid, uint8_t addr) {
    uint8_t d[9]; d[0] = CMD_ENROLL_ASSIGN; memcpy(&d[1], uid, 7); d[8] = addr;
    sendCmd(ENROLL_ADDR, d, 9);
}
void replyPending(const uint8_t *uid) {
    uint8_t d[8]; d[0] = CMD_ENROLL_PENDING; memcpy(&d[1], uid, 7);
    sendCmd(ENROLL_ADDR, d, 8);
}

bool waitAck(uint32_t timeout_ms) {
    uint32_t t0 = millis();
    while (millis() - t0 < timeout_ms) {
        if (radio.available()) {
            uint8_t raw[NRF_PAYLOAD_SIZE];
            radio.read(raw, sizeof(raw));
            if (raw[2] == CMD_ACK) return true;
        }
    }
    return false;
}

/* Reliable send: transmit, wait for the display's ACK, retransmit on
 * timeout. The display ACKs every command, so lost draw packets are retried
 * instead of silently dropped (which caused partial frames). Duplicate
 * commands are harmless (same text/rect at the same place). */
bool sendReliable(uint8_t addr, const uint8_t *data, uint8_t n,
                  uint32_t ackTimeout, uint8_t tries) {
    for (uint8_t t = 0; t < tries; t++) {
        modeTx(); writeCmd(addr, data, n); modeRx();
        if (waitAck(ackTimeout)) {
            delay(15);   /* let the display return to RX before the next cmd */
            return true;
        }
    }
    return false;
}

bool cClear(uint8_t addr, uint8_t color) {
    uint8_t d[2] = {CMD_CLEAR, color};
    return sendReliable(addr, d, 2, 500, 6);
}
bool cRect(uint8_t addr, uint16_t x, uint8_t y, uint16_t w, uint8_t h, uint8_t color) {
    uint8_t d[8] = {CMD_RECT,(uint8_t)(x>>8),(uint8_t)x,y,(uint8_t)(w>>8),(uint8_t)w,h,color};
    return sendReliable(addr, d, 8, 500, 6);
}
bool cText(uint8_t addr, uint16_t x, uint8_t y, uint8_t scale, uint8_t color, const char *s) {
    uint8_t d[NRF_PAYLOAD_SIZE]; uint8_t n = 0;
    d[n++] = CMD_TEXT; d[n++] = (uint8_t)(x>>8); d[n++] = (uint8_t)x;
    d[n++] = y; d[n++] = scale; d[n++] = color;
    while (*s && n < 29) d[n++] = (uint8_t)(*s++);
    return sendReliable(addr, d, n, 500, 6);
}

void sendFrame(uint8_t idx) {
    uint8_t addr = reg[idx].addr;
    String lbl = hexBytes(reg[idx].label, 4, 0);

    Serial.printf("[%lu] frame -> Addr 0x%02X (%s)...\n", millis(), addr, lbl.c_str());

    bool ok = true;
    ok &= cClear(addr, WHITE);
    ok &= cRect(addr, 0, 0, 296, 24, RED);
    ok &= cText(addr, 70, 6, 2, WHITE, "HA DISPLAY");
    ok &= cText(addr, 10, 40, 2, BLACK, "Wetter: 18C Sonne");
    ok &= cText(addr, 10, 68, 2, BLACK, "PV: 2.4 kW");
    ok &= cText(addr, 10, 96, 2, BLACK, "Netz: OK");
    ok &= cText(addr, 10, 128, 1, BLACK, lbl.c_str());

    if (!ok) Serial.printf("[%lu]   WARN: nicht alle Kommandos bestaetigt\n", millis());

    /* UPDATE: the display wakes, renders and refreshes the e-ink (can take
     * ~30s) before it ACKs - give it plenty of time. */
    uint8_t up[1] = {CMD_UPDATE};
    if (sendReliable(addr, up, 1, 60000, 2))
        Serial.printf("[%lu]   refresh OK\n", millis());
    else
        Serial.printf("[%lu]   kein ACK (refresh?)\n", millis());

    lastFrame[idx] = millis();
}

/* ---------- enroll handling ---------- */
void handleReq(const uint8_t *uid, const uint8_t *label) {
    int idx = findReg(uid);
    if (idx >= 0) {
        replyAssign(reg[idx].uid, reg[idx].addr);
        if (!online[idx]) {
            online[idx] = true;
            lastFrame[idx] = millis() - FRAME_INTERVAL;   /* send a frame soon */
            Serial.printf("[online] Label %s  Addr 0x%02X\n",
                          hexBytes(reg[idx].label,4,0).c_str(), reg[idx].addr);
        }
        return;
    }
    if (findPend(uid) < 0 && pendCount < MAX_PENDING) {
        memcpy(pend[pendCount].uid, uid, 7);
        memcpy(pend[pendCount].label, label, 4);
        pendCount++;
        Serial.printf("\n*** NEUES DISPLAY ***  Label %s  UID %s\n",
                      hexBytes(label,4,0).c_str(), hexBytes(uid,7,':').c_str());
        Serial.println("    -> 'y' freigeben, 'n' ablehnen\n");
    }
    replyPending(uid);
}

void approveOldest() {
    if (pendCount == 0) { Serial.println("(keine wartenden)"); return; }
    if (regCount >= MAX_ENTRIES) { Serial.println("Registry voll!"); return; }
    uint8_t addr = nextFreeAddr();
    if (!addr) { Serial.println("Keine freie Adresse!"); return; }
    memcpy(reg[regCount].uid, pend[0].uid, 7);
    reg[regCount].addr = addr;
    memcpy(reg[regCount].label, pend[0].label, 4);
    online[regCount] = false; lastFrame[regCount] = 0;
    regCount++;
    saveReg();
    Serial.printf("FREIGEGEBEN: Label %s -> Addr 0x%02X\n",
                  hexBytes(pend[0].label,4,0).c_str(), addr);
    for (uint8_t i = 1; i < pendCount; i++) pend[i-1] = pend[i];
    pendCount--;
}
void rejectOldest() {
    if (pendCount == 0) { Serial.println("(keine wartenden)"); return; }
    Serial.printf("ABGELEHNT: Label %s\n", hexBytes(pend[0].label,4,0).c_str());
    for (uint8_t i = 1; i < pendCount; i++) pend[i-1] = pend[i];
    pendCount--;
}
void listAll() {
    Serial.printf("\n--- Registry (%u) ---\n", regCount);
    for (uint8_t i = 0; i < regCount; i++)
        Serial.printf("  Addr 0x%02X  Label %s  UID %s  %s\n",
                      reg[i].addr, hexBytes(reg[i].label,4,0).c_str(),
                      hexBytes(reg[i].uid,7,':').c_str(), online[i] ? "online" : "-");
    Serial.printf("--- Pending (%u) ---\n", pendCount);
    for (uint8_t i = 0; i < pendCount; i++)
        Serial.printf("  Label %s\n", hexBytes(pend[i].label,4,0).c_str());
    Serial.println();
}
void handleSerial() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c=='y'||c=='Y') approveOldest();
        else if (c=='n'||c=='N') rejectOldest();
        else if (c=='l'||c=='L') listAll();
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("\n=== PriceTag Gateway (integrated) ===");

    prefs.begin("pricetag", false);
    loadReg();
    Serial.printf("Registry: %u Eintraege ('l' listen)\n", regCount);

    SPI.begin(3, 9, 7, CSN_PIN);
    if (!radio.begin(&SPI)) { Serial.println("ERROR: nRF24!"); while (1) delay(1000); }
    radio.setChannel(NRF_CHANNEL);
    radio.setDataRate(RF24_250KBPS);
    radio.setPALevel(RF24_PA_MAX);
    radio.setPayloadSize(NRF_PAYLOAD_SIZE);
    radio.setAutoAck(false);
    radio.setRetries(0, 0);
    radio.disableDynamicPayloads();
    radio.disableCRC();
    modeRx();

    Serial.println("Bereit.\n");
}

void loop() {
    handleSerial();

    if (radio.available()) {
        uint8_t raw[NRF_PAYLOAD_SIZE];
        radio.read(raw, sizeof(raw));
        uint8_t len = raw[1];
        if (len >= 1 && (uint8_t)(len + 3) <= NRF_PAYLOAD_SIZE) {
            uint8_t x = 0; for (uint8_t i = 0; i < len + 2; i++) x ^= raw[i];
            if (x == raw[len + 2] && raw[2] == CMD_ENROLL_REQ && len >= 12)
                handleReq(&raw[3], &raw[10]);
        }
    }

    /* Drive online displays. */
    for (uint8_t i = 0; i < regCount; i++) {
        if (online[i] && (millis() - lastFrame[i] >= FRAME_INTERVAL))
            sendFrame(i);
    }
}
