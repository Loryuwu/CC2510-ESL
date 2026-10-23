/*
 * CC2510 Debug Diagnose-Sketch für ESP32-S3 Zero
 *
 * Flashe diesen Sketch STATT CCLib_proxy.ino
 * Öffne den Serial Monitor auf 115200 Baud
 * Er zeigt genau was passiert und wo es hängt
 */

// ===== Pin Configuration =====
int CC_RST = 6;
int CC_DC  = 4;
int CC_DD  = 5;
// ==============================

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  delay(1000);

  Serial.println("=== CC2510 Debug Diagnose ===");
  Serial.printf("Pins: DD=GPIO%d, DC=GPIO%d, RST=GPIO%d\n", CC_DD, CC_DC, CC_RST);
  Serial.println();

  // --- Test 1: GPIO funktioniert? ---
  Serial.println("[Test 1] GPIO-Pins konfigurieren...");
  pinMode(CC_DC, OUTPUT);
  pinMode(CC_RST, OUTPUT);
  pinMode(CC_DD, INPUT);
  digitalWrite(CC_DC, LOW);
  digitalWrite(CC_RST, HIGH);  // RST nicht aktiv
  delay(100);

  int ddState = digitalRead(CC_DD);
  Serial.printf("  DD Pin liest: %d (erwartet: 0 oder 1, nicht immer gleich)\n", ddState);
  Serial.println("  -> Wenn DD immer 0 oder immer 1: Kabel pruefen oder Pin falsch");
  Serial.println();

  // --- Test 2: RST Toggle ---
  Serial.println("[Test 2] RST toggle test...");
  digitalWrite(CC_RST, HIGH);
  delay(10);
  digitalWrite(CC_RST, LOW);
  delay(10);
  int dd_rst_low = digitalRead(CC_DD);
  digitalWrite(CC_RST, HIGH);
  delay(10);
  int dd_rst_high = digitalRead(CC_DD);
  Serial.printf("  DD bei RST=LOW:  %d\n", dd_rst_low);
  Serial.printf("  DD bei RST=HIGH: %d\n", dd_rst_high);
  Serial.println();

  // --- Test 3: Enter Debug Mode ---
  Serial.println("[Test 3] Enter Debug Mode Sequenz...");

  // DD und DC auf LOW
  pinMode(CC_DD, OUTPUT);
  digitalWrite(CC_DD, LOW);
  digitalWrite(CC_DC, LOW);
  delay(1);

  // RST runter
  Serial.println("  RST -> LOW");
  digitalWrite(CC_RST, LOW);
  delay(10);

  // 2x DC pulse (das ist die Debug-Entry Sequenz)
  Serial.println("  DC Pulse 1: HIGH-LOW");
  digitalWrite(CC_DC, HIGH);
  delayMicroseconds(50);
  digitalWrite(CC_DC, LOW);
  delayMicroseconds(50);

  Serial.println("  DC Pulse 2: HIGH-LOW");
  digitalWrite(CC_DC, HIGH);
  delayMicroseconds(50);
  digitalWrite(CC_DC, LOW);
  delay(10);

  // RST hoch
  Serial.println("  RST -> HIGH");
  digitalWrite(CC_RST, HIGH);
  delay(10);

  Serial.println("  Debug Mode entry gesendet!");
  Serial.println();

  // --- Test 4: Chip ID lesen ---
  Serial.println("[Test 4] GET_CHIP_ID senden (0x68)...");

  // Write 0x68 (GET_CHIP_ID) - bit-bang MSB first
  pinMode(CC_DD, OUTPUT);
  byte cmd = 0x68;
  for (int i = 7; i >= 0; i--) {
    digitalWrite(CC_DD, (cmd >> i) & 1 ? HIGH : LOW);
    delayMicroseconds(20);
    digitalWrite(CC_DC, HIGH);
    delayMicroseconds(20);
    digitalWrite(CC_DC, LOW);
    delayMicroseconds(20);
  }
  Serial.println("  Kommando gesendet");

  // Switch to read - wait for DD to go LOW
  pinMode(CC_DD, INPUT);
  delayMicroseconds(20);

  Serial.printf("  DD nach switchRead: %d ", digitalRead(CC_DD));

  int waitCount = 0;
  bool timeout = false;
  while (digitalRead(CC_DD) == HIGH) {
    // Clock 8 pulses
    for (int j = 0; j < 8; j++) {
      digitalWrite(CC_DC, HIGH);
      delayMicroseconds(20);
      digitalWrite(CC_DC, LOW);
      delayMicroseconds(20);
    }
    waitCount++;
    if (waitCount > 255) {
      timeout = true;
      break;
    }
  }

  if (timeout) {
    Serial.println("-> TIMEOUT! Chip antwortet nicht.");
    Serial.println();
    Serial.println("=== DIAGNOSE ===");
    Serial.println("Der Chip antwortet nicht auf Debug-Kommandos.");
    Serial.println("Moegliche Ursachen:");
    Serial.println("  1) Falsche Pins - pruefe ob DD/DC/RST am CC2510 stimmen");
    Serial.println("  2) CC2510 bekommt keinen Strom (VCC/GND angeschlossen?)");
    Serial.println("  3) CC2510 ist kein CC2510 (anderer Chip?)");
    Serial.println("  4) Debug-Pads auf dem Board nicht verbunden");
    Serial.println("  5) Kalte Loetstelle");
    Serial.println();
    Serial.println("Tipp: Miss mit Multimeter:");
    Serial.println("  - VCC am CC2510: sollte ~3.3V sein");
    Serial.println("  - RST am CC2510: sollte ~3.3V sein (HIGH)");
    Serial.println("  - Durchgang zwischen ESP-Pin und CC2510-Pad");
  } else {
    Serial.printf("-> READY nach %d Zyklen!\n", waitCount);

    // Read 2 bytes (chip ID)
    byte chipHi = 0;
    byte chipLo = 0;

    for (int i = 7; i >= 0; i--) {
      digitalWrite(CC_DC, HIGH);
      delayMicroseconds(20);
      if (digitalRead(CC_DD)) chipHi |= (1 << i);
      digitalWrite(CC_DC, LOW);
      delayMicroseconds(20);
    }
    for (int i = 7; i >= 0; i--) {
      digitalWrite(CC_DC, HIGH);
      delayMicroseconds(20);
      if (digitalRead(CC_DD)) chipLo |= (1 << i);
      digitalWrite(CC_DC, LOW);
      delayMicroseconds(20);
    }

    unsigned short chipID = (chipHi << 8) | chipLo;
    Serial.printf("  Chip ID: 0x%04X (Hi=0x%02X Lo=0x%02X)\n", chipID, chipHi, chipLo);

    if (chipID == 0x0000) {
      Serial.println("  -> 0x0000 = Keine Antwort / nicht verbunden");
    } else if (chipID == 0xFFFF) {
      Serial.println("  -> 0xFFFF = Kurzschluss / falsche Verdrahtung");
    } else if ((chipID & 0xFF00) == 0x8100) {
      Serial.println("  -> CC2510 erkannt! Alles OK!");
    } else if ((chipID & 0xFF00) == 0xA500) {
      Serial.println("  -> CC2530 erkannt");
    } else {
      Serial.printf("  -> Unbekannter Chip (0x%04X)\n", chipID);
    }

    // Read debug status
    Serial.println();
    Serial.println("[Test 5] READ_STATUS senden (0x34 fuer CC2510 / 0x30 fuer CC254x)...");

    // Try CC2510 status command (0x34)
    pinMode(CC_DD, OUTPUT);
    cmd = 0x34;
    for (int i = 7; i >= 0; i--) {
      digitalWrite(CC_DD, (cmd >> i) & 1 ? HIGH : LOW);
      delayMicroseconds(20);
      digitalWrite(CC_DC, HIGH);
      delayMicroseconds(20);
      digitalWrite(CC_DC, LOW);
      delayMicroseconds(20);
    }

    pinMode(CC_DD, INPUT);
    delayMicroseconds(20);

    waitCount = 0;
    timeout = false;
    while (digitalRead(CC_DD) == HIGH) {
      for (int j = 0; j < 8; j++) {
        digitalWrite(CC_DC, HIGH);
        delayMicroseconds(20);
        digitalWrite(CC_DC, LOW);
        delayMicroseconds(20);
      }
      waitCount++;
      if (waitCount > 255) { timeout = true; break; }
    }

    if (!timeout) {
      byte status = 0;
      for (int i = 7; i >= 0; i--) {
        digitalWrite(CC_DC, HIGH);
        delayMicroseconds(20);
        if (digitalRead(CC_DD)) status |= (1 << i);
        digitalWrite(CC_DC, LOW);
        delayMicroseconds(20);
      }
      Serial.printf("  Debug Status: 0x%02X\n", status);
      if (status & 0x04) Serial.println("  *** DEBUG_LOCKED = 1 -> Readout Protection AKTIV! ***");
      if (status & 0x20) Serial.println("  CPU_HALTED = 1");
      if (status & 0x02) Serial.println("  OSCILLATOR_STABLE = 1");
    }
  }

  Serial.println();
  Serial.println("=== Diagnose fertig ===");
}

void loop() {
  delay(10000);
}
