/*
 * PriceTag CC2510 - NFC I2C Bus Scanner (discovery tool)
 *
 * Goal: identify the board's NFC chip so we can read its factory UID (the
 * per-display identity) and later use its EEPROM to store the assigned radio
 * address.
 *
 * The CC2510 has no USB serial, so results are reported over the proven
 * CC->ESP radio path. Run gateway_rx_test.ino (or gateway_loopback) on the
 * ESP and read the raw hex. Each scan sends one packet:
 *
 *   data = [0xB5][fd][count][addr0][addr1]...    (7-bit I2C addresses that ACK)
 *
 * gateway_rx_test prints the raw hex; look for the bytes after B5:
 *   fd    = NFC field-detect pin state (P1.1)
 *   count = number of responding I2C addresses
 *   addrN = the 7-bit addresses (e.g. 0x55 = NXP NTAG I2C, 0x53/0x57 = ST25)
 *
 * I2C is bit-banged, open-drain emulated: '0' drives the pin low, '1'
 * releases it to the board's pull-up (ESL boards populate I2C pull-ups). If
 * NOTHING ACKs, likely causes: no pull-ups, NFC not powered, or wrong pins.
 */

#include <cc2510fx.h>
#include "radio.h"

#define BV(x) (1 << (x))

extern void delay_ms(uint16_t ms);

#define SDA_BIT 4          /* P0.4 */
#define SCL_BIT 6          /* P0.6 */
#define NFC_VCC_BIT 0      /* P1.0 (also powers SPI flash) */
#define NFC_FD_BIT  1      /* P1.1 field detect (input) */

static void i2c_delay(void) {
    volatile uint8_t i;
    for (i = 0; i < 60; i++) { __asm nop __endasm; }
}

/* Open-drain emulation on P0: low = drive 0, high = release to pull-up. */
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

/* Write a byte, return 1 if the slave ACKed (SDA low on the 9th clock). */
static uint8_t i2c_write(uint8_t b) {
    uint8_t i, nack;
    for (i = 0; i < 8; i++) {
        if (b & 0x80) sda_hi(); else sda_lo();
        i2c_delay();
        scl_hi(); i2c_delay();
        scl_lo(); i2c_delay();
        b <<= 1;
    }
    sda_hi();                 /* release for ACK */
    i2c_delay();
    scl_hi(); i2c_delay();
    nack = sda_read();        /* 0 = ACK, 1 = NACK */
    scl_lo(); i2c_delay();
    return nack ? 0 : 1;
}

static __xdata uint8_t rep[20];

void main(void) {
    uint8_t a, count, fd, i;

    radio_init(0x01);

    /* Power the NFC chip (P1.0 high), FD as input. Give it time to boot. */
    P1DIR |= BV(NFC_VCC_BIT);
    P1DIR &= (uint8_t)~BV(NFC_FD_BIT);
    P1_0 = 1;
    delay_ms(50);

    /* I2C pins to GPIO, idle high. */
    P0SEL &= (uint8_t)~(BV(SDA_BIT) | BV(SCL_BIT));
    sda_hi();
    scl_hi();
    i2c_delay();

    for (;;) {
        count = 0;
        for (a = 0x08; a <= 0x77; a++) {
            i2c_start();
            if (i2c_write((uint8_t)(a << 1))) {     /* write address */
                if (count < 16) rep[3 + count] = a;
                count++;
            }
            i2c_stop();
        }

        fd = P1_1 ? 1 : 0;

        rep[0] = 0xB5;        /* scan-result marker */
        rep[1] = fd;
        rep[2] = count;
        /* addresses already placed at rep[3..] */

        i = (uint8_t)(3 + (count < 16 ? count : 16));
        radio_tx(rep, i);

        delay_ms(1000);
    }
}
