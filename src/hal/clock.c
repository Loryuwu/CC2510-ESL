#include "hal.h"

void init_clock(void) {
  CLKCON = 0x80; // 32 KHz clock osc, 26MHz crystal osc.

  // wait for selection to be active
  while (!CLOCKSOURCE_XOSC_STABLE()) {
  }
  NOP();

  // El oscilador no utilizado (HS RCOSC) se apaga automáticamente al entrar en PM2.
  // No es necesario modificar el registro SLEEP aquí.
}