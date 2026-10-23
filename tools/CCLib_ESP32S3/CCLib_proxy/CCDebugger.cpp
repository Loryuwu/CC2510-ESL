/**
 * CC-Debugger Protocol Library - ESP32-S3 Version
 * Based on CCLib by Ioannis Charalampidis & Simon Schulz
 * Adapted for ESP32-S3 (3.3V native, single DD pin)
 *
 * Licensed under GNU/GPLv3
 */

#include "CCDebugger.h"

#define INSTR_VERSION   0
#define I_HALT          1
#define I_RESUME        2
#define I_RD_CONFIG     3
#define I_WR_CONFIG     4
#define I_DEBUG_INSTR_1 5
#define I_DEBUG_INSTR_2 6
#define I_DEBUG_INSTR_3 7
#define I_GET_CHIP_ID   8
#define I_GET_PC        9
#define I_READ_STATUS   10
#define I_STEP_INSTR    11
#define I_CHIP_ERASE    12

// Bit-bang clock timing — matches the working diagnostic sketch
#define CC_CLOCK_US 20

CCDebugger::CCDebugger( int pinRST, int pinDC, int pinDD )
{
  this->pinRST = pinRST;
  this->pinDC  = pinDC;
  this->pinDD  = pinDD;
  this->pinLED = -1;
  this->ddIsOutput = 0xFF;  // force first setDDDirection to actually run
  this->inDebugMode = 0;
  this->errorFlag = CC_ERROR_NONE;

  pinMode(pinDC,  OUTPUT);
  pinMode(pinRST, OUTPUT);
  digitalWrite(pinDC,  LOW);
  digitalWrite(pinRST, LOW);

  setDDDirection(INPUT);

  // Default instruction set for CC254x (updated via Python for CC2510)
  instr[INSTR_VERSION]    = 1;
  instr[I_HALT]           = 0x40;
  instr[I_RESUME]         = 0x48;
  instr[I_RD_CONFIG]      = 0x20;
  instr[I_WR_CONFIG]      = 0x18;
  instr[I_DEBUG_INSTR_1]  = 0x51;
  instr[I_DEBUG_INSTR_2]  = 0x52;
  instr[I_DEBUG_INSTR_3]  = 0x53;
  instr[I_GET_CHIP_ID]    = 0x68;
  instr[I_GET_PC]         = 0x28;
  instr[I_READ_STATUS]    = 0x30;
  instr[I_STEP_INSTR]     = 0x58;
  instr[I_CHIP_ERASE]     = 0x10;

  active = true;
}

void CCDebugger::setLED( int pinLED )
{
  this->pinLED = pinLED;
  if (pinLED >= 0) {
    pinMode(pinLED, OUTPUT);
    digitalWrite(pinLED, LOW);
  }
}

void CCDebugger::setActive( boolean on )
{
  errorFlag = CC_ERROR_NONE;
  if (on == this->active) return;
  this->active = on;

  if (on) {
    pinMode(pinDC,  OUTPUT);
    pinMode(pinDD,  INPUT);
    pinMode(pinRST, OUTPUT);
    digitalWrite(pinDC,  LOW);
    digitalWrite(pinRST, LOW);
    if (pinLED >= 0) {
      pinMode(pinLED, OUTPUT);
      digitalWrite(pinLED, LOW);
    }
    setDDDirection(INPUT);
  } else {
    if (inDebugMode) this->exit();
    pinMode(pinDC,  INPUT);
    pinMode(pinDD,  INPUT);
    pinMode(pinRST, INPUT);
    if (pinLED >= 0) {
      pinMode(pinLED, INPUT);
    }
  }
}

byte CCDebugger::error()
{
  return errorFlag;
}

byte CCDebugger::enter()
{
  if (!active) { errorFlag = CC_ERROR_NOT_ACTIVE; return 0; }
  if (pinLED >= 0) digitalWrite(pinLED, HIGH);

  errorFlag = CC_ERROR_NONE;

  // CC debug entry: hold RST low, pulse DC twice, release RST
  setDDDirection(OUTPUT);
  digitalWrite(pinDD, LOW);
  digitalWrite(pinDC, LOW);

  digitalWrite(pinRST, LOW);
  delay(10);
  digitalWrite(pinDC, HIGH);
  delayMicroseconds(50);
  digitalWrite(pinDC, LOW);
  delayMicroseconds(50);
  digitalWrite(pinDC, HIGH);
  delayMicroseconds(50);
  digitalWrite(pinDC, LOW);
  delay(10);
  digitalWrite(pinRST, HIGH);
  delay(10);

  inDebugMode = 1;

  if (pinLED >= 0) digitalWrite(pinLED, LOW);
  return 0;
}

byte CCDebugger::write( byte data )
{
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  if (pinLED >= 0) digitalWrite(pinLED, HIGH);

  setDDDirection(OUTPUT);

  for (byte cnt = 8; cnt; cnt--) {
    digitalWrite(pinDD, (data & 0x80) ? HIGH : LOW);
    delayMicroseconds(CC_CLOCK_US);
    digitalWrite(pinDC, HIGH);
    delayMicroseconds(CC_CLOCK_US);
    digitalWrite(pinDC, LOW);
    delayMicroseconds(CC_CLOCK_US);
    data <<= 1;
  }

  if (pinLED >= 0) digitalWrite(pinLED, LOW);
  return 0;
}

byte CCDebugger::switchRead(byte maxWaitCycles)
{
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  if (pinLED >= 0) digitalWrite(pinLED, HIGH);

  byte didWait = 0;
  setDDDirection(INPUT);
  delayMicroseconds(CC_CLOCK_US);

  while (digitalRead(pinDD) == HIGH) {
    for (byte cnt = 8; cnt; cnt--) {
      digitalWrite(pinDC, HIGH);
      delayMicroseconds(CC_CLOCK_US);
      digitalWrite(pinDC, LOW);
      delayMicroseconds(CC_CLOCK_US);
    }
    didWait = 1;
    if (!--maxWaitCycles) {
      errorFlag = CC_ERROR_NOT_WIRED;
      inDebugMode = 0;
      if (pinLED >= 0) digitalWrite(pinLED, LOW);
      return 0;
    }
  }

  if (didWait) delayMicroseconds(CC_CLOCK_US);
  if (pinLED >= 0) digitalWrite(pinLED, LOW);
  return 0;
}

byte CCDebugger::switchWrite()
{
  setDDDirection(OUTPUT);
  return 0;
}

byte CCDebugger::read()
{
  if (!active) { errorFlag = CC_ERROR_NOT_ACTIVE; return 0; }
  if (pinLED >= 0) digitalWrite(pinLED, HIGH);

  byte data = 0;
  setDDDirection(INPUT);

  for (byte cnt = 8; cnt; cnt--) {
    digitalWrite(pinDC, HIGH);
    delayMicroseconds(CC_CLOCK_US);
    data <<= 1;
    if (digitalRead(pinDD) == HIGH) data |= 0x01;
    digitalWrite(pinDC, LOW);
    delayMicroseconds(CC_CLOCK_US);
  }

  if (pinLED >= 0) digitalWrite(pinLED, LOW);
  return data;
}

void CCDebugger::setDDDirection( byte direction )
{
  if (direction == ddIsOutput) return;
  ddIsOutput = direction;

  if (ddIsOutput) {
    pinMode(pinDD, OUTPUT);
    digitalWrite(pinDD, LOW);
  } else {
    pinMode(pinDD, INPUT);
  }
}

byte CCDebugger::exit()
{
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }

  write( instr[I_RESUME] );
  switchRead();
  read();
  switchWrite();
  inDebugMode = 0;
  return 0;
}

byte CCDebugger::getConfig() {
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  write( instr[I_RD_CONFIG] );
  switchRead();
  byte bAns = read();
  switchWrite();
  return bAns;
}

byte CCDebugger::setConfig( byte config ) {
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  write( instr[I_WR_CONFIG] );
  write( config );
  switchRead();
  byte bAns = read();
  switchWrite();
  return bAns;
}

byte CCDebugger::exec( byte oc0 )
{
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  write( instr[I_DEBUG_INSTR_1] );
  write( oc0 );
  switchRead();
  byte bAns = read();
  switchWrite();
  return bAns;
}

byte CCDebugger::exec( byte oc0, byte oc1 )
{
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  write( instr[I_DEBUG_INSTR_2] );
  write( oc0 );
  write( oc1 );
  switchRead();
  byte bAns = read();
  switchWrite();
  return bAns;
}

byte CCDebugger::exec( byte oc0, byte oc1, byte oc2 )
{
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  write( instr[I_DEBUG_INSTR_3] );
  write( oc0 );
  write( oc1 );
  write( oc2 );
  switchRead();
  byte bAns = read();
  switchWrite();
  return bAns;
}

byte CCDebugger::execi( byte oc0, unsigned short c0 )
{
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  write( instr[I_DEBUG_INSTR_3] );
  write( oc0 );
  write( (c0 >> 8) & 0xFF );
  write(  c0 & 0xFF );
  switchRead();
  byte bAns = read();
  switchWrite();
  return bAns;
}

unsigned short CCDebugger::getChipID() {
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  write( instr[I_GET_CHIP_ID] );
  switchRead();
  unsigned short bAns = (unsigned short)read() << 8;
  bAns |= read();
  switchWrite();
  return bAns;
}

unsigned short CCDebugger::getPC() {
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  write( instr[I_GET_PC] );
  switchRead();
  unsigned short bAns = (unsigned short)read() << 8;
  bAns |= read();
  switchWrite();
  return bAns;
}

byte CCDebugger::getStatus() {
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  write( instr[I_READ_STATUS] );
  switchRead();
  byte bAns = read();
  switchWrite();
  return bAns;
}

byte CCDebugger::step() {
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  write( instr[I_STEP_INSTR] );
  switchRead();
  byte bAns = read();
  switchWrite();
  return bAns;
}

byte CCDebugger::resume() {
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  write( instr[I_RESUME] );
  switchRead();
  byte bAns = read();
  switchWrite();
  return bAns;
}

byte CCDebugger::halt() {
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  write( instr[I_HALT] );
  switchRead();
  byte bAns = read();
  switchWrite();
  return bAns;
}

byte CCDebugger::chipErase()
{
  if (!active)      { errorFlag = CC_ERROR_NOT_ACTIVE;    return 0; }
  if (!inDebugMode) { errorFlag = CC_ERROR_NOT_DEBUGGING; return 0; }
  write( instr[I_CHIP_ERASE] );
  switchRead();
  byte bAns = read();
  switchWrite();
  return bAns;
}

byte CCDebugger::updateInstructionTable( byte newTable[16] )
{
  for (byte i = 0; i < 16; i++)
    instr[i] = newTable[i];
  return instr[INSTR_VERSION];
}

byte CCDebugger::getInstructionTableVersion()
{
  return instr[INSTR_VERSION];
}
