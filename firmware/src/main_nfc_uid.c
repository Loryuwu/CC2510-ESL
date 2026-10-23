/*
 * PriceTag CC2510 - NFC UID reader (NXP NT3H2111 / NTAG I2C plus 2k)
 *
 * Reads block 0 (16 bytes) from the NTAG I2C plus at I2C address 0x55 and
 * reports it over the proven CC->ESP radio path. Block 0 holds the factory
 * data; the 7-byte UID is block0[0,1,2,4,5,6,7] (block0[3] = BCC0 check byte).
 * block0[0] must be 0x04 (NXP manufacturer code) - use that to confirm a
 * valid read.
 *
 * Report packet (read in gateway_rx_test raw hex):
 *   data = [0xB7][present][block0[0..15]]      (18 bytes)
 *     present = 1 if the chip ACKed its address, else 0
 * So in the raw hex "01 LEN B7 PP <16 bytes> XOR":
 *     UID = bytes at block0 offsets 0,1,2,4,5,6,7  (first should be 04)
 *
 * NTAG I2C read: START, [0x55 W], [blockaddr=0x00], repeated START,
 * [0x55 R], read 16 bytes (ACK x15, NACK last), STOP.
 */

#include <cc2510fx.h>
#include "radio.h"

#define BV(x) (1 << (x))

extern void delay_ms(uint16_t ms);

#define SDA_BIT 4          /* P0.4 */
#define SCL_BIT 6          /* P0.6 */
#define NFC_VCC_BIT 0      /* P1.0 */

#define NTAG_ADDR 0x55     /* 7-bit I2C address */

static void i2c_delay(void) {
    volatile uint8_t i;
    for (i = 0; i < 60; i++) { __asm nop __endasm; }
}

static void sda_hi(void) { P0DIR &= (uint8_t)~BV(SDA_BIT); }
static void sda_lo(void) { P0DIR |= BV(SDA_BIT); P0_4 = 0; }
static void scl_hi(void) { P0DIR &= (uint8_t)~BV(SCL_BIT); }
static void scl_lo(void) { P0DIR |= BV(SCL_BIT); P0_6 = 0; }
static uint8_t sda_read(void) {
    P0DIR &= (uint8_t)~BV(SDA_BIT);
    return P0_4 ? 1 : 0;
}

static void i2c_start(void) {
    sda_hi(); scl_hi(); i2c_delay();
    sda_lo(); i2c_delay();
    scl_lo(); i2c_delay();
}
static void i2c_stop(void) {
    sda_lo(); scl_lo(); i2c_delay();
    scl_hi(); i2c_delay();
    sda_hi(); i2c_delay();
}
static uint8_t i2c_write(uint8_t b) {
    uint8_t i, nack;
    for (i = 0; i < 8; i++) {
        if (b & 0x80) sda_hi(); else sda_lo();
        i2c_delay();
        scl_hi(); i2c_delay();
        scl_lo(); i2c_delay();
        b <<= 1;
    }
    sda_hi(); i2c_delay();
    scl_hi(); i2c_delay();
    nack = sda_read();
    scl_lo(); i2c_delay();
    return nack ? 0 : 1;      /* 1 = ACK */
}
static uint8_t i2c_read(uint8_t send_ack) {
    uint8_t i, b = 0;
    sda_hi();                 /* release so slave drives SDA */
    for (i = 0; i < 8; i++) {
        b <<= 1;
        scl_hi(); i2c_delay();
        if (sda_read()) b |= 1;
        scl_lo(); i2c_delay();
    }
    if (send_ack) sda_lo(); else sda_hi();
    i2c_delay();
    scl_hi(); i2c_delay();
    scl_lo(); i2c_delay();
    sda_hi();
    return b;
}

static __xdata uint8_t rep[20];

void main(void) {
    uint8_t present, i;

    radio_init(0x01);

    P1DIR |= BV(NFC_VCC_BIT);
    P1_0 = 1;
    delay_ms(50);

    P0SEL &= (uint8_t)~(BV(SDA_BIT) | BV(SCL_BIT));
    sda_hi(); scl_hi();
    i2c_delay();

    for (;;) {
        for (i = 0; i < 16; i++) rep[2 + i] = 0;

        i2c_start();
        present = i2c_write((uint8_t)(NTAG_ADDR << 1));   /* write mode */
        if (present) {
            i2c_write(0x00);                               /* block 0 */
            i2c_start();                                   /* repeated start */
            i2c_write((uint8_t)((NTAG_ADDR << 1) | 1));    /* read mode */
            for (i = 0; i < 16; i++)
                rep[2 + i] = i2c_read(i < 15 ? 1 : 0);     /* ACK x15, NACK last */
        }
        i2c_stop();

        rep[0] = 0xB7;
        rep[1] = present;
        radio_tx(rep, 18);

        delay_ms(1000);
    }
}
