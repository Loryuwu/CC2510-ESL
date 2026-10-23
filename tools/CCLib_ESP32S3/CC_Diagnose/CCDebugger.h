/**
 * CC-Debugger Protocol Library - ESP32-S3 Version
 * Based on CCLib by Ioannis Charalampidis & Simon Schulz
 * Adapted for ESP32-S3 (3.3V native, single DD pin)
 *
 * Licensed under GNU/GPLv3
 */

#ifndef CCDEBUGGER_H
#define CCDEBUGGER_H

#define CC_ERROR_NONE           0
#define CC_ERROR_NOT_ACTIVE     1
#define CC_ERROR_NOT_DEBUGGING  2
#define CC_ERROR_NOT_WIRED      3

#include "Arduino.h"

class CCDebugger {
public:

  CCDebugger( int pinRST, int pinDC, int pinDD );
  void setLED( int pinLED );
  void setActive( boolean on );
  byte error();

  byte enter();
  byte exit();

  byte exec( byte oc0 );
  byte exec( byte oc0, byte oc1 );
  byte exec( byte oc0, byte oc1, byte oc2 );
  byte execi( byte oc0, unsigned short c0 );

  unsigned short getChipID();
  unsigned short getPC();
  byte getStatus();
  byte resume();
  byte halt();
  byte step();
  byte getConfig();
  byte setConfig( byte config );
  byte chipErase();

  byte write( byte data );
  byte switchRead( byte maxWaitCycles = 255 );
  byte switchWrite();
  byte read();
  byte updateInstructionTable( byte newTable[16] );
  byte getInstructionTableVersion();

private:

  void setDDDirection( byte direction );

  byte instr[16];
  int  pinRST;
  int  pinDC;
  int  pinDD;
  int  pinLED;
  byte errorFlag;
  byte ddIsOutput;
  byte inDebugMode;
  boolean active;
};

#endif
