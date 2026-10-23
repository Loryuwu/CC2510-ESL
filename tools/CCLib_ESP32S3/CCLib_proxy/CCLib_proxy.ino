/*
 * CCLib Proxy - ESP32-S3 Zero Version
 * Rewritten with inline bit-bang (same code as working diagnostic)
 *
 * Wiring ESP32-S3 Zero -> CC2510:
 *   GPIO 5  ->  DD  (P2.1, Debug Data)
 *   GPIO 4  ->  DC  (P2.2, Debug Clock)
 *   GPIO 6  ->  RST (Reset)
 *   3.3V    ->  VCC
 *   GND     ->  GND
 */

// ===== Pin Configuration =====
#define CC_RST 6
#define CC_DC  4
#define CC_DD  5
// ==============================

#define CMD_ENTER     0x01
#define CMD_EXIT      0x02
#define CMD_CHIP_ID   0x03
#define CMD_STATUS    0x04
#define CMD_PC        0x05
#define CMD_STEP      0x06
#define CMD_EXEC_1    0x07
#define CMD_EXEC_2    0x08
#define CMD_EXEC_3    0x09
#define CMD_BRUSTWR   0x0A
#define CMD_RD_CFG    0x0B
#define CMD_WR_CFG    0x0C
#define CMD_CHPERASE  0x0D
#define CMD_RESUME    0x0E
#define CMD_HALT      0x0F
#define CMD_PING      0xF0
#define CMD_INSTR_VER 0xF1
#define CMD_INSTR_UPD 0xF2

#define ANS_OK    0x01
#define ANS_ERROR 0x02
#define ANS_READY 0x03

/* Debug bit-bang half-period in microseconds. 20 was wildly conservative
 * (~25 kHz debug clock); the CC2510 debug interface handles far faster. 4us
 * is ~4-5x faster end-to-end and still safe. If you ever see flaky flashing,
 * nudge this back up (6, 8, ...). The Python writer's chunk retries cover the
 * occasional glitch regardless. */
#define DLY 4

byte instr[16];
bool inDebugMode = false;

// ---- Low-level bit-bang (identical to diagnostic sketch) ----

void ccWrite(byte data) {
  pinMode(CC_DD, OUTPUT);
  for (int i = 7; i >= 0; i--) {
    digitalWrite(CC_DD, (data >> i) & 1 ? HIGH : LOW);
    delayMicroseconds(DLY);
    digitalWrite(CC_DC, HIGH);
    delayMicroseconds(DLY);
    digitalWrite(CC_DC, LOW);
    delayMicroseconds(DLY);
  }
}

bool ccSwitchRead() {
  pinMode(CC_DD, INPUT);
  delayMicroseconds(DLY);
  int waitCount = 0;
  while (digitalRead(CC_DD) == HIGH) {
    for (int j = 0; j < 8; j++) {
      digitalWrite(CC_DC, HIGH);
      delayMicroseconds(DLY);
      digitalWrite(CC_DC, LOW);
      delayMicroseconds(DLY);
    }
    if (++waitCount > 255) return false;
  }
  return true;
}

byte ccRead() {
  byte data = 0;
  pinMode(CC_DD, INPUT);
  for (int i = 7; i >= 0; i--) {
    digitalWrite(CC_DC, HIGH);
    delayMicroseconds(DLY);
    if (digitalRead(CC_DD)) data |= (1 << i);
    digitalWrite(CC_DC, LOW);
    delayMicroseconds(DLY);
  }
  return data;
}

void ccSwitchWrite() {
  pinMode(CC_DD, OUTPUT);
}

// ---- Debug entry (identical to diagnostic sketch) ----

void ccEnter() {
  pinMode(CC_DD, OUTPUT);
  digitalWrite(CC_DD, LOW);
  digitalWrite(CC_DC, LOW);
  delay(1);

  digitalWrite(CC_RST, LOW);
  delay(10);
  digitalWrite(CC_DC, HIGH);
  delayMicroseconds(50);
  digitalWrite(CC_DC, LOW);
  delayMicroseconds(50);
  digitalWrite(CC_DC, HIGH);
  delayMicroseconds(50);
  digitalWrite(CC_DC, LOW);
  delay(10);
  digitalWrite(CC_RST, HIGH);
  delay(10);

  inDebugMode = true;
}

// ---- Protocol helpers ----

void sendFrame(byte ans, byte b0 = 0, byte b1 = 0) {
  Serial.write(ans);
  Serial.write(b1);
  Serial.write(b0);
  Serial.flush();
}

// Write command byte, wait for ready, read one response byte
byte ccCommand1(byte cmd) {
  ccWrite(cmd);
  if (!ccSwitchRead()) return 0;
  byte ans = ccRead();
  ccSwitchWrite();
  return ans;
}

// Write command + 1 data byte, read one response byte
byte ccCommand2(byte cmd, byte d1) {
  ccWrite(cmd);
  ccWrite(d1);
  if (!ccSwitchRead()) return 0;
  byte ans = ccRead();
  ccSwitchWrite();
  return ans;
}

// Write command + 2 data bytes, read one response byte
byte ccCommand3(byte cmd, byte d1, byte d2) {
  ccWrite(cmd);
  ccWrite(d1);
  ccWrite(d2);
  if (!ccSwitchRead()) return 0;
  byte ans = ccRead();
  ccSwitchWrite();
  return ans;
}

// Write command + 3 data bytes, read one response byte
byte ccCommand4(byte cmd, byte d1, byte d2, byte d3) {
  ccWrite(cmd);
  ccWrite(d1);
  ccWrite(d2);
  ccWrite(d3);
  if (!ccSwitchRead()) return 0;
  byte ans = ccRead();
  ccSwitchWrite();
  return ans;
}

// Write command, read two response bytes (16-bit)
unsigned short ccCommand1_word(byte cmd) {
  ccWrite(cmd);
  if (!ccSwitchRead()) return 0;
  byte hi = ccRead();
  byte lo = ccRead();
  ccSwitchWrite();
  return ((unsigned short)hi << 8) | lo;
}

// ---- Setup ----

void setup() {
  pinMode(CC_DC, OUTPUT);
  pinMode(CC_RST, OUTPUT);
  digitalWrite(CC_DC, LOW);
  digitalWrite(CC_RST, LOW);

  // Default instruction set (CC254x, updated for CC2510 via Python)
  instr[0]  = 1;    // version
  instr[1]  = 0x40; // HALT
  instr[2]  = 0x48; // RESUME
  instr[3]  = 0x20; // RD_CONFIG
  instr[4]  = 0x18; // WR_CONFIG
  instr[5]  = 0x51; // DEBUG_INSTR_1
  instr[6]  = 0x52; // DEBUG_INSTR_2
  instr[7]  = 0x53; // DEBUG_INSTR_3
  instr[8]  = 0x68; // GET_CHIP_ID
  instr[9]  = 0x28; // GET_PC
  instr[10] = 0x30; // READ_STATUS
  instr[11] = 0x58; // STEP_INSTR
  instr[12] = 0x10; // CHIP_ERASE

  Serial.begin(115200);
  while (!Serial) { delay(10); }
  delay(500);

  ccEnter();
}

// ---- Main loop ----

void loop() {
  if (Serial.available() < 4) return;

  byte cmd = Serial.read();
  byte c1  = Serial.read();
  byte c2  = Serial.read();
  byte c3  = Serial.read();

  if (cmd == CMD_PING) {
    sendFrame(ANS_OK);

  } else if (cmd == CMD_ENTER) {
    ccEnter();
    sendFrame(ANS_OK);

  } else if (cmd == CMD_EXIT) {
    ccCommand1(instr[2]); // RESUME
    inDebugMode = false;
    sendFrame(ANS_OK);

  } else if (cmd == CMD_CHIP_ID) {
    unsigned short id = ccCommand1_word(instr[8]);
    sendFrame(ANS_OK, id & 0xFF, (id >> 8) & 0xFF);

  } else if (cmd == CMD_PC) {
    unsigned short pc = ccCommand1_word(instr[9]);
    sendFrame(ANS_OK, pc & 0xFF, (pc >> 8) & 0xFF);

  } else if (cmd == CMD_STATUS) {
    byte s = ccCommand1(instr[10]);
    sendFrame(ANS_OK, s);

  } else if (cmd == CMD_HALT) {
    byte s = ccCommand1(instr[1]);
    sendFrame(ANS_OK, s);

  } else if (cmd == CMD_EXEC_1) {
    byte s = ccCommand2(instr[5], c1);
    sendFrame(ANS_OK, s);

  } else if (cmd == CMD_EXEC_2) {
    byte s = ccCommand3(instr[6], c1, c2);
    sendFrame(ANS_OK, s);

  } else if (cmd == CMD_EXEC_3) {
    byte s = ccCommand4(instr[7], c1, c2, c3);
    sendFrame(ANS_OK, s);

  } else if (cmd == CMD_BRUSTWR) {
    int iLen = (c1 << 8) | c2;
    if (iLen > 2048) {
      sendFrame(ANS_ERROR, 3);
      return;
    }

    sendFrame(ANS_READY);

    ccWrite(0x80 | (c1 & 0x07));
    ccWrite(c2);

    int iRead = iLen;
    byte bIdle = 0;
    while (iRead > 0) {
      if (Serial.available() >= 1) {
        ccWrite(Serial.read());
        bIdle = 0;
        iRead--;
      } else {
        if (++bIdle > 200) {
          while (iRead > 0) { ccWrite(0); iRead--; }
          ccSwitchRead();
          byte s = ccRead();
          ccSwitchWrite();
          sendFrame(ANS_ERROR, 4);
          return;
        }
        delay(50);
      }
    }

    ccSwitchRead();
    byte s = ccRead();
    ccSwitchWrite();
    sendFrame(ANS_OK, s);

  } else if (cmd == CMD_RD_CFG) {
    byte s = ccCommand1(instr[3]);
    sendFrame(ANS_OK, s);

  } else if (cmd == CMD_WR_CFG) {
    byte s = ccCommand2(instr[4], c1);
    sendFrame(ANS_OK, s);

  } else if (cmd == CMD_CHPERASE) {
    byte s = ccCommand1(instr[12]);
    sendFrame(ANS_OK, s);

  } else if (cmd == CMD_STEP) {
    byte s = ccCommand1(instr[11]);
    sendFrame(ANS_OK, s);

  } else if (cmd == CMD_RESUME) {
    byte s = ccCommand1(instr[2]);
    sendFrame(ANS_OK, s);

  } else if (cmd == CMD_INSTR_VER) {
    sendFrame(ANS_OK, instr[0]);

  } else if (cmd == CMD_INSTR_UPD) {
    sendFrame(ANS_READY);
    byte buf[16];
    int i = 0;
    while (i < 16) {
      if (Serial.available() >= 1) {
        buf[i++] = Serial.read();
      }
    }
    for (int j = 0; j < 16; j++) instr[j] = buf[j];
    ccEnter();
    sendFrame(ANS_OK, instr[0]);

  } else {
    sendFrame(ANS_ERROR, 0xFF);
  }
}
