#include "hal.h"

void init_clock(void) {
  // Select 26MHz Crystal Oscillator (XOSC) as system clock source
  // Bit 7 (OSC) = 0 -> XOSC
  // Bit 0 (OSC32) = 0 -> 32kHz XOSC (optional, but keeping default)
  // CLKSPD = 000 -> 26MHz
  // TICKSPD = 000 -> 26MHz
  // Bit 0 (OSC32) = 1 -> 32kHz RCOSC (Safe default)
  CLKCON = 0x80; 

  // wait for selection to be active
  while (!CLOCKSOURCE_XOSC_STABLE()) {
  }
  NOP();

  // power down the unused oscillator (RCOSC)
  SLEEP |= 0x04;
 
}