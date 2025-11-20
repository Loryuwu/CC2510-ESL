#ifndef _TIME_H_
#define _TIME_H_

#include "hal.h"
#include <stdint.h>
#include "clock.h"

void time_init(void);
uint32_t millis(void);
void delay_ms(uint16_t millis);

void sleep_ms(uint32_t ms);
void deep_sleep_seconds(uint32_t seconds); //deep sleep for a number of seconds
void deep_sleep_minutes(uint32_t minutes); //deep sleep for a number of minutes
void deep_sleep_hours(uint32_t hours); //deep sleep for a number of hours

#endif