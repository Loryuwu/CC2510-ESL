/*
 * CC2510 Chip Erase + Verify
 * Tests if debug instructions work IMMEDIATELY after erase
 * (without re-entering debug mode), like imagotag-hack does.
 *
 * Wiring: GPIO5=DD, GPIO4=DC, GPIO6=RST, 3.3V=VCC, GND=GND
 */

#define CC_RST 6
#define CC_DC  4
#define CC_DD  5
#define RW_DELAY 1

#define I_CHIP_ERASE    0x14
#define I_READ_STATUS   0x34
#define I_GET_CHIP_ID   0x68
#define I_DEBUG_INSTR_1 0x55
#define I_DEBUG_INSTR_3 0x57

void cc_send(uint8_t data) {
  pinMode(CC_DD, OUTPUT);
  pinMode(CC_DC, OUTPUT);
  for (uint8_t bct = 8; bct; bct--) {
    digitalWrite(CC_DD, (data & 0x80) ? HIGH : LOW);
    digitalWrite(CC_DC, HIGH);
    data <<= 1;
    delayMicroseconds(RW_DELAY);
    digitalWrite(CC_DC, LOW);
    delayMicroseconds(RW_DELAY);
  }
  pinMode(CC_DD, INPUT);
  pinMode(CC_DC, INPUT);
}

uint8_t cc_readyWait() {
  uint8_t bct = 255;
  pinMode(CC_DD, INPUT);
  pinMode(CC_DC, OUTPUT);
  digitalWrite(CC_DC, LOW);
  while (--bct) {
    if (digitalRead(CC_DD) == LOW) break;
  }
  return bct;
}

uint8_t cc_read() {
  uint8_t data = 0;
  pinMode(CC_DD, INPUT);
  pinMode(CC_DC, OUTPUT);
  for (uint8_t bct = 8; bct; bct--) {
    digitalWrite(CC_DC, HIGH);
    delayMicroseconds(RW_DELAY);
    data <<= 1;
    if (digitalRead(CC_DD) == HIGH) data |= 0x01;
    digitalWrite(CC_DC, LOW);
    delayMicroseconds(RW_DELAY);
  }
  pinMode(CC_DD, INPUT);
  pinMode(CC_DC, INPUT);
  return data;
}

void cc_enter() {
  digitalWrite(CC_RST, LOW);
  delayMicroseconds(200);
  pinMode(CC_DC, OUTPUT);
  digitalWrite(CC_DC, HIGH);
  delayMicroseconds(10);
  digitalWrite(CC_DC, LOW);
  delayMicroseconds(10);
  digitalWrite(CC_DC, HIGH);
  delayMicroseconds(10);
  digitalWrite(CC_DC, LOW);
  delayMicroseconds(100);
  digitalWrite(CC_RST, HIGH);
  delayMicroseconds(10);
}

uint8_t cc_readStatus() {
  cc_send(I_READ_STATUS);
  if (cc_readyWait()) return cc_read();
  return 0xFF;
}

uint16_t cc_getChipID() {
  cc_send(I_GET_CHIP_ID);
  if (!cc_readyWait()) return 0;
  uint8_t hi = cc_read();
  uint8_t lo = cc_read();
  return ((uint16_t)hi << 8) | lo;
}

uint8_t cc_exec1(uint8_t opcode) {
  cc_send(I_DEBUG_INSTR_1);
  cc_send(opcode);
  uint8_t w = cc_readyWait();
  if (w) return cc_read();
  Serial.print("  [exec1 0x");
  Serial.print(opcode, HEX);
  Serial.println(" TIMEOUT]");
  return 0;
}

uint8_t cc_exec3(uint8_t oc0, uint8_t oc1, uint8_t oc2) {
  cc_send(I_DEBUG_INSTR_3);
  cc_send(oc0);
  cc_send(oc1);
  cc_send(oc2);
  uint8_t w = cc_readyWait();
  if (w) return cc_read();
  Serial.print("  [exec3 TIMEOUT]");
  return 0;
}

uint8_t cc_eraseChip() {
  uint8_t x = 255, status = 0x00;
  cc_send(I_CHIP_ERASE);
  while (x--) {
    status = cc_readStatus();
    if (status & 0x80) break;
  }
  return x;
}

void printStatus(uint8_t s) {
  Serial.print("  0x");
  Serial.print(s, HEX);
  Serial.print(" [");
  if (s & 0x80) Serial.print("DONE ");
  if (s & 0x20) Serial.print("HALT ");
  if (s & 0x04) Serial.print("LOCK ");
  if (s & 0x02) Serial.print("OSC ");
  Serial.println("]");
}

bool testDebugInstr() {
  Serial.println("  Trying CLR A (0xE4)...");
  cc_exec1(0xE4);

  Serial.println("  Trying MOV DPTR,#0x0000...");
  cc_exec3(0x90, 0x00, 0x00);

  Serial.println("  Trying CLR A...");
  cc_exec1(0xE4);

  Serial.println("  Trying MOVC A,@A+DPTR...");
  uint8_t val = cc_exec1(0x93);
  Serial.print("  Flash[0]: 0x");
  Serial.println(val, HEX);
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  delay(1000);

  pinMode(CC_DC, INPUT);
  pinMode(CC_DD, INPUT);
  pinMode(CC_RST, OUTPUT);
  digitalWrite(CC_RST, LOW);
  delay(1);
  digitalWrite(CC_RST, HIGH);

  Serial.println("=== CC2510 Erase + Verify ===\n");

  // Enter debug
  Serial.println("[1] Enter debug mode");
  cc_enter();
  delay(10);

  uint16_t id = cc_getChipID();
  Serial.print("  Chip ID: 0x");
  Serial.println(id, HEX);

  uint8_t status = cc_readStatus();
  Serial.print("  Status: ");
  printStatus(status);

  // Test debug instructions BEFORE erase (should fail if locked)
  Serial.println("\n[2] Test debug instructions BEFORE erase:");
  testDebugInstr();

  // Erase
  Serial.println("\n[3] Chip Erase...");
  uint8_t result = cc_eraseChip();
  Serial.print("  Polls: ");
  Serial.println(255 - result);

  // Test IMMEDIATELY after erase - NO re-enter!
  Serial.println("\n[4] Status IMMEDIATELY after erase (NO re-enter):");
  status = cc_readStatus();
  printStatus(status);

  Serial.println("\n[5] Test debug instructions IMMEDIATELY after erase:");
  testDebugInstr();

  // Now re-enter and test again
  Serial.println("\n[6] Re-enter debug mode...");
  cc_enter();
  delay(10);

  status = cc_readStatus();
  Serial.print("  Status: ");
  printStatus(status);

  Serial.println("\n[7] Test debug instructions after re-enter:");
  testDebugInstr();

  // Normal boot then debug
  Serial.println("\n[8] Normal boot (RST without debug entry)...");
  pinMode(CC_DC, INPUT);
  pinMode(CC_DD, INPUT);
  digitalWrite(CC_RST, LOW);
  delay(200);
  digitalWrite(CC_RST, HIGH);
  delay(2000);

  Serial.println("[9] Enter debug after normal boot...");
  cc_enter();
  delay(10);

  status = cc_readStatus();
  Serial.print("  Status: ");
  printStatus(status);

  Serial.println("\n[10] Test debug instructions after boot+enter:");
  testDebugInstr();

  Serial.println("\n=== Done ===");
}

void loop() {
  delay(10000);
}
