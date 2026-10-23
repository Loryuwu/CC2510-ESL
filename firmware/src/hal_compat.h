/*
 * hal_compat.h  —  kleiner Kompatibilitaets-Shim.
 *
 * Die aus dem urspruenglichen Loryuwu-Fork uebernommenen Zusatzmodule
 * (flash, uart, cobs) brauchten dessen hal.h (Makros + Registerheader). Dieser
 * Shim stellt genau diese Makros bereit und bindet den SDCC-Registerheader
 * <cc2510fx.h> ein, so wie es auch radio.c tut. So kompilieren die Module in
 * der PriceTag-Firmware ohne die alte HAL-Struktur.
 */

#ifndef HAL_COMPAT_H
#define HAL_COMPAT_H

#include <cc2510fx.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef BV
#define BV(x) (1 << (x))
#endif

/* "statement"-Wrapper: fasst mehrere Anweisungen zu einem Ausdruck zusammen. */
#define st(x) do { x } while (0)

#define HAL_ENABLE_INTERRUPTS()  st(EA = 1;)
#define HAL_DISABLE_INTERRUPTS() st(EA = 0;)

typedef unsigned char halIntState_t;
#define HAL_ENTER_CRITICAL_SECTION(s) st(s = EA; HAL_DISABLE_INTERRUPTS();)
#define HAL_EXIT_CRITICAL_SECTION(s)  st(EA = s;)
#define HAL_CRITICAL_STATEMENT(x) \
  st(halIntState_t _s; HAL_ENTER_CRITICAL_SECTION(_s); x; HAL_EXIT_CRITICAL_SECTION(_s);)

#ifndef INTERRUPT
#define INTERRUPT(name, vector) void name(void) __interrupt(vector)
#endif

/* Verzoegerung aus delay.c (bzw. epd.c) - dieselbe wie im uebrigen Projekt. */
extern void delay_ms(uint16_t ms);

#endif /* HAL_COMPAT_H */
