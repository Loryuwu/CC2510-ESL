#ifndef _LED_H_
#define _LED_H_

#include "hal.h"
#include <stdint.h>

#define LED_BOOST_ON P2_2 = 1
#define LED_BOOST_OFF P2_2 = 0

#define LED_ON P2_1 = 1
#define LED_OFF P2_1 = 0
#define LED_TOGGLE P2_1 ^= 1

#define LED_R_ON  P1_5 = 1
#define LED_R_OFF P1_5 = 0
#define LED_R_TOGGLE P1_5 ^= 1

#define LED_B_ON  P1_6 = 1
#define LED_B_OFF P1_6 = 0
#define LED_B_TOGGLE P1_6 ^= 1

#define LED_G_ON  P1_7 = 1
#define LED_G_OFF P1_7 = 0
#define LED_G_TOGGLE P1_7 ^= 1

#define LED_INIT            \
  {                         \
    P2DIR |= BV(1) | BV(2); \
    P1DIR |= BV(5) | BV(6) | BV(7); \
    LED_BOOST_ON;        \
    LED_OFF;             \
    LED_R_OFF; \
    LED_B_OFF; \
    LED_G_OFF; \
  }

void led_init();

#endif /* _LED_H_ */