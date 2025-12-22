#include "time.h"

#define SLEEP_TIMER_TICKS_PER_SECOND 32768UL
#define SLEEP_TIMER_MAX_TICKS 0xFFFFUL
#define SLEEP_TIMER_MAX_SECONDS 1UL
#define SLEEP_TIMER_MAX_MS (SLEEP_TIMER_MAX_SECONDS * 1000UL)

static volatile uint32_t currentTime = 0;

static void disable_millis_timer(void);
static void deep_sleep_prepare(void);
static void deep_sleep_complete(void);
static void enter_pm2_for_ticks(uint32_t ticks);
static uint32_t saturating_mul(uint32_t value, uint32_t factor);

/***************************
 *Timer3: genera millis()
 ***************************/

INTERRUPT(timer3_isr, T3_VECTOR) {
  T3OVFIF = 0; // clear overflow flag
  currentTime += 1;
}

INTERRUPT(sleep_timer_isr, ST_VECTOR) {
  STIF = 0;
  WORIRQ &= ~0x01; // Clear Event 0 flag
}

void time_init(void) {
  T3CTL = (uint8_t)((0b111 << 5) | BV(1) | BV(3)); // clk/128, modulo mode, overflow interrupt en
  T3CC0 = 203;                                     // ~1ms intrerval
  T3IE = BV(0);                                    // enable timer3 interrupt
  T3CTL |= BV(4);                                  // start Timer 3
}

volatile uint32_t millis(void) {
  uint32_t value;
  HAL_CRITICAL_STATEMENT(value = currentTime);
  return value;
}

void delay_ms(uint16_t milliseconds) {
  uint32_t start = millis();
  while (millis() - start < milliseconds) {
  }
}

/***************************
 *Timer1: sleep_for_minutes()
 ***************************/


void sleep_ms(uint32_t ms) {
  if (ms == 0) {
    return;
  }

  deep_sleep_prepare();

  uint32_t remaining = ms;
  while (remaining > 0) {
    uint32_t chunk = (remaining > SLEEP_TIMER_MAX_MS) ? SLEEP_TIMER_MAX_MS : remaining;
    uint32_t ticks = (chunk * SLEEP_TIMER_TICKS_PER_SECOND) / 1000UL;
    if (ticks == 0) {
      ticks = 1;
    }
    enter_pm2_for_ticks(ticks);
    remaining -= chunk;
  }

  deep_sleep_complete();
}

static void disable_millis_timer(void) {
  T3CTL = 0x00;
  T3IE = 0;
  T3OVFIF = 0;
}

static void deep_sleep_prepare(void) {
  HAL_DISABLE_INTERRUPTS();
  disable_millis_timer();
  HAL_ENABLE_INTERRUPTS();
}

static void deep_sleep_complete(void) {
  init_clock();
  time_init();
}

static uint32_t saturating_mul(uint32_t value, uint32_t factor) {
  if (factor == 0) {
    return 0;
  }
  if (value > (0xFFFFFFFFUL / factor)) {
    return 0xFFFFFFFFUL;
  }
  return value * factor;
}

static void enter_pm2_for_ticks(uint32_t ticks) {
  if (ticks == 0) {
    return;
  }

  halIntState_t istate;
  HAL_ENTER_CRITICAL_SECTION(istate);

  // Set Sleep Timer resolution to 1 tick = 1/32768 s (Resolution 0)
  WORCTRL = 0x00; 

  // Set Event 0 timeout
  WOREVT0 = (uint8_t)(ticks & 0xFF);
  WOREVT1 = (uint8_t)((ticks >> 8) & 0xFF);

  // Reset Sleep Timer (Event 0) to synchronize
  WORCTRL |= 0x01; 

  // Enable Event 0 interrupt mask and clear flag
  WORIRQ |= 0x10; 
  WORIRQ &= ~0x01;

  STIF = 0;
  STIE = 1;

  HAL_EXIT_CRITICAL_SECTION(istate);

  // Enter PM2
  SLEEP &= ~0x03;
  SLEEP |= 0x02;
  PCON |= 0x01;
  
  __asm nop __endasm;
  __asm nop __endasm;
  __asm nop __endasm;

  STIE = 0;
  HAL_ENABLE_INTERRUPTS();
}

void deep_sleep_seconds(uint32_t seconds) {
  if (seconds == 0) {
    return;
  }

  deep_sleep_prepare();

  uint32_t remaining = seconds;
  while (remaining > 0) {
    uint32_t chunk = (remaining > SLEEP_TIMER_MAX_SECONDS) ? SLEEP_TIMER_MAX_SECONDS : remaining;
    uint32_t ticks = chunk * SLEEP_TIMER_TICKS_PER_SECOND;
    enter_pm2_for_ticks(ticks);
    remaining -= chunk;
  }

  deep_sleep_complete();
}

void deep_sleep_minutes(uint32_t minutes) {
  uint32_t seconds = saturating_mul(minutes, 60UL);
  deep_sleep_seconds(seconds);
}

void deep_sleep_hours(uint32_t hours) {
  uint32_t seconds = saturating_mul(hours, 3600UL);
  deep_sleep_seconds(seconds);
}



