#ifndef _TIME_H_
#define _TIME_H_

#include "hal.h"
#include <stdint.h>
#include "clock.h"

void time_init();
uint32_t millis();
void delay_ms(uint16_t millis);
void wor_sleep_60s();
void sleep_ms(uint32_t ms);

#endif