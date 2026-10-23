/*
 * PriceTag Gateway - Milestone C: enrollment + registry (with approval)
 *
 * Partner sketch for firmware/src/main_enroll.c. Displays announce themselves
 * with their NFC UID + printed label id; the gateway owns the UID->address
 * registry (persisted in NVS, survives display re-flash). New displays land in
 * a pending list and you approve them over the serial console.
 *
 * Serial commands:
 *   y        approve the oldest pending display (assigns next free address)
 *   n        reject (drop) the oldest pending display
 *   l        list the registry and pending queue
 *   f <uid>  forget a registry entry (uid as 14 hex chars) - optional
 *
 * Protocol:
 *   ENROLL_REQ     0xE0 [uid0..6][lbl0..3]   display -> gateway  (src 0x00)
 *   ENROLL_ASSIGN  0xE1 [uid0..6][addr]      gateway -> display  (dst 0x00)
 *   ENROLL_PENDING 0xE2 [uid0..6]            gateway -> display  (dst 0x00)
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

#define ENROLL_ADDR        0x00
#define CMD_ENROLL_REQ     0xE0
#define CMD_ENROLL_ASSIGN  0xE1
#define CMD_ENROLL_PENDING 0xE2

const uint8_t txAddr[5] = {0xA7, 0x5C, 0xA7, 0xAA, 0xAA};  /* on-air AA AA A7 5C A7 */
const uint8_t rxAddr[3] = {0xA7, 0x5C, 0xA7};

RF24 radio(CE_PIN, CSN_PIN);
Preferences prefs;

#define MAX_ENTRIES 16
struct Entry { uint8_t uid[7]; uint8_t addr; uint8_t label[4]; };
Entry   reg[MAX_ENTRIES];
uint8_t regCount = 0;
bool    online[MAX_ENTRIES];        /* printed-online-this-session flags */

#define MAX_PENDING 8
struct Pend { uint8_t uid[7]; uint8_t label[4]; };
Pend    pend[MAX_PENDING];
uint8_t pendCount = 0;

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
    return 0;   /* full */
}
void saveReg() { prefs.putBytes("reg", reg, regCount * sizeof(Entry)); }
void loadReg() {
    size_t n = prefs.getBytes("reg", reg, sizeof(reg));
    regCount = n / sizeof(Entry);
    for (uint8_t i = 0; i < MAX_ENTRIES; i++) online[i] = false;
}

/* ---------- nRF24 TX/RX switching ---------- */
uint8_t readReg(uint8_t r){ uint8_t v; digitalWrite(CSN_PIN,LOW); SPI.transfer(r&0x1F); v=SPI.transfer(0xFF); digitalWrite(CSN_PIN,HIGH); return v; }
void writeReg(uint8_t r,uint8_t v){ digitalWrite(CSN_PIN,LOW); SPI.transfer(0x20|(r&0x1F)); SPI.transfer(v); digitalWrite(CSN_PIN,HIGH); }
void forceCrcOff(){ writeReg(0x00, readReg(0x00) & ~0x0C); }
void switchToTx(){ radio.stopListening(); radio.setAddressWidth(5); radio.openWritingPipe(txAddr); }
void switchToRx(){ radio.setAddressWidth(3); radio.openReadingPipe(0, rxAddr); radio.startListening(); forceCrcOff(); }

/* Send a command to dest address: [dest][n][data...][xor]. */
void sendCmdTo(uint8_t dest, const uint8_t *data, uint8_t n) {
    uint8_t p[NRF_PAYLOAD_SIZE];
    memset(p, 0, sizeof(p));
    p[0] = dest; p[1] = n;
    memcpy(&p[2], data, n);
    uint8_t x = 0; for (uint8_t i = 0; i < n + 2; i++) x ^= p[i];
    p[n + 2] = x;
    switchToTx();
    radio.write(p, sizeof(p));
    switchToRx();
}
void replyAssign(const uint8_t *uid, uint8_t addr) {
    uint8_t d[9]; d[0] = CMD_ENROLL_ASSIGN;
    memcpy(&d[1], uid, 7); d[8] = addr;
    sendCmdTo(ENROLL_ADDR, d, 9);
}
void replyPending(const uint8_t *uid) {
    uint8_t d[8]; d[0] = CMD_ENROLL_PENDING;
    memcpy(&d[1], uid, 7);
    sendCmdTo(ENROLL_ADDR, d, 8);
}

/* ---------- enroll request handling ---------- */
void handleReq(const uint8_t *uid, const uint8_t *label) {
    int idx = findReg(uid);
    if (idx >= 0) {
        replyAssign(reg[idx].uid, reg[idx].addr);
        if (!online[idx]) {
            online[idx] = true;
            Serial.printf("[online] Label %s  UID %s  -> Addr 0x%02X\n",
                          hexBytes(reg[idx].label,4,0).c_str(),
                          hexBytes(reg[idx].uid,7,':').c_str(), reg[idx].addr);
        }
        return;
    }
    /* unknown -> pending */
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
    if (pendCount == 0) { Serial.println("(keine wartenden Displays)"); return; }
    if (regCount >= MAX_ENTRIES) { Serial.println("Registry voll!"); return; }
    uint8_t addr = nextFreeAddr();
    if (!addr) { Serial.println("Keine freie Adresse!"); return; }
    memcpy(reg[regCount].uid, pend[0].uid, 7);
    reg[regCount].addr = addr;
    memcpy(reg[regCount].label, pend[0].label, 4);
    online[regCount] = false;
    regCount++;
    saveReg();
    Serial.printf("FREIGEGEBEN: Label %s -> Addr 0x%02X\n",
                  hexBytes(pend[0].label,4,0).c_str(), addr);
    for (uint8_t i = 1; i < pendCount; i++) pend[i-1] = pend[i];
    pendCount--;
}
void rejectOldest() {
    if (pendCount == 0) { Serial.println("(keine wartenden Displays)"); return; }
    Serial.printf("ABGELEHNT: Label %s\n", hexBytes(pend[0].label,4,0).c_str());
    for (uint8_t i = 1; i < pendCount; i++) pend[i-1] = pend[i];
    pendCount--;
}
void listAll() {
    Serial.printf("\n--- Registry (%u) ---\n", regCount);
    for (uint8_t i = 0; i < regCount; i++)
        Serial.printf("  Addr 0x%02X  Label %s  UID %s  %s\n",
                      reg[i].addr, hexBytes(reg[i].label,4,0).c_str(),
                      hexBytes(reg[i].uid,7,':').c_str(),
                      online[i] ? "online" : "-");
    Serial.printf("--- Pending (%u) ---\n", pendCount);
    for (uint8_t i = 0; i < pendCount; i++)
        Serial.printf("  Label %s  UID %s\n",
                      hexBytes(pend[i].label,4,0).c_str(),
                      hexBytes(pend[i].uid,7,':').c_str());
    Serial.println();
}

void handleSerial() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == 'y' || c == 'Y') approveOldest();
        else if (c == 'n' || c == 'N') rejectOldest();
        else if (c == 'l' || c == 'L') listAll();
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("\n=== PriceTag Gateway - Enrollment ===");

    prefs.begin("pricetag", false);
    loadReg();
    Serial.printf("Registry geladen: %u Eintraege. ('l' listen)\n", regCount);

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
    switchToRx();

    Serial.println("Warte auf Anmeldungen...\n");
}

void loop() {
    handleSerial();

    if (radio.available()) {
        uint8_t raw[NRF_PAYLOAD_SIZE];
        radio.read(raw, sizeof(raw));
        /* raw = [src][len][cmd][...]; verify XOR over [0..len+1] vs raw[len+2] */
        uint8_t len = raw[1];
        if (len >= 1 && (uint8_t)(len + 3) <= NRF_PAYLOAD_SIZE) {
            uint8_t x = 0; for (uint8_t i = 0; i < len + 2; i++) x ^= raw[i];
            if (x == raw[len + 2] && raw[2] == CMD_ENROLL_REQ && len >= 12)
                handleReq(&raw[3], &raw[10]);
        }
    }
}
