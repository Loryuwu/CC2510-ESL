#ifndef CC2510_H
#define CC2510_H

#include <stdint.h>

// Try SDCC's built-in header first
#ifdef BUILD
#include <cc2510fx.h>
#else
// Fallback SFR definitions for CC2510

// Port 0
__sfr __at(0x80) P0;
__sbit __at(0x80) P0_0;
__sbit __at(0x81) P0_1;
__sbit __at(0x83) P0_3;
__sbit __at(0x85) P0_5;

// Port 1
__sfr __at(0x90) P1;
__sbit __at(0x92) P1_2;
__sbit __at(0x93) P1_3;

// Port 2
__sfr __at(0xA0) P2;
__sbit __at(0xA0) P2_0;
__sbit __at(0xA1) P2_1;
__sbit __at(0xA2) P2_2;

// Interrupt enable
__sfr __at(0xA8) IEN0;
__sbit __at(0xAF) EA;

// Port direction
__sfr __at(0xFD) P0DIR;
__sfr __at(0xFE) P1DIR;
__sfr __at(0xFF) P2DIR;

// Port function select
__sfr __at(0xF3) P0SEL;
__sfr __at(0xF4) P1SEL;
__sfr __at(0xF5) P2SEL;

// Clock control
__sfr __at(0xC6) CLKCON;
__sfr __at(0xBE) SLEEP;

// Memory control
__sfr __at(0xC7) MEMCTR;

// USART 0
__sfr __at(0x86) U0CSR;
__sfr __at(0xC4) U0GCR;
__sfr __at(0xC5) U0BAUD;
__sfr __at(0xC1) U0DBUF;

// Timer 3
__sfr __at(0xCB) T3CTL;
__sfr __at(0xCC) T3CC0;
__sfr __at(0xC3) T3IE_SFR; // actually part of IEN1
// T3 overflow flag in T3CTL or IRCON
__sfr __at(0xC0) IRCON;

// Peripheral config
__sfr __at(0xF1) PERCFG;

// IEN1 for timer3 interrupt enable
__sfr __at(0xB8) IEN1;
__sbit __at(0xBB) T3IE;

// Timer 3 overflow flag
__sbit __at(0xC0) T3OVFIF; // IRCON bit 0 is T3IF? Check

#define NOP() __asm nop __endasm
#define BV(x) (1 << (x))

// Interrupt vector definitions
#define T3_VECTOR 19

#define INTERRUPT(name, vector) \
  void name(void) __interrupt(vector)

#endif // BUILD

#ifndef BV
#define BV(x) (1 << (x))
#endif

#ifndef NOP
#define NOP() __asm nop __endasm
#endif

#ifndef INTERRUPT
#define INTERRUPT(name, vector) \
  void name(void) __interrupt(vector)
#endif

#endif // CC2510_H
