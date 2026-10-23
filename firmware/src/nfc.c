#include <cc2510fx.h>
#include "nfc.h"

#define BV(x) (1 << (x))

extern void delay_ms(uint16_t ms);

#define SDA_BIT 4          /* P0.4 */
#define SCL_BIT 6          /* P0.6 */
#define NFC_VCC_BIT 0      /* P1.0 (also powers SPI flash) */

#define NTAG_ADDR 0x55     /* 7-bit I2C address */

static __xdata uint8_t nfc_xbuf[16];

/* ---------- bit-banged I2C (open-drain emulated via board pull-ups) ------- */

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
    sda_hi();
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

/* ---------- public API ---------- */

void nfc_init(void) {
    P1DIR |= BV(NFC_VCC_BIT);
    P1_0 = 1;                 /* power the NFC chip */
    delay_ms(50);             /* boot time */

    P0SEL &= (uint8_t)~(BV(SDA_BIT) | BV(SCL_BIT));   /* GPIO */
    sda_hi(); scl_hi();
    i2c_delay();
}

uint8_t nfc_read_block(uint8_t block, uint8_t *buf16) {
    uint8_t i;

    i2c_start();
    if (!i2c_write((uint8_t)(NTAG_ADDR << 1))) { i2c_stop(); return 0; }
    i2c_write(block);
    i2c_start();                                   /* repeated start */
    if (!i2c_write((uint8_t)((NTAG_ADDR << 1) | 1))) { i2c_stop(); return 0; }
    for (i = 0; i < 16; i++)
        buf16[i] = i2c_read(i < 15 ? 1 : 0);       /* ACK x15, NACK last */
    i2c_stop();
    return 1;
}

uint8_t nfc_write_block(uint8_t block, const uint8_t *buf16) {
    uint8_t i;

    i2c_start();
    if (!i2c_write((uint8_t)(NTAG_ADDR << 1))) { i2c_stop(); return 0; }
    i2c_write(block);
    for (i = 0; i < 16; i++)
        i2c_write(buf16[i]);
    i2c_stop();

    delay_ms(10);             /* EEPROM commit (~4ms typ) */
    return 1;
}

uint8_t nfc_read_uid(uint8_t *uid7) {
    if (!nfc_read_block(0, nfc_xbuf)) return 0;
    uid7[0] = nfc_xbuf[0];
    uid7[1] = nfc_xbuf[1];
    uid7[2] = nfc_xbuf[2];
    uid7[3] = nfc_xbuf[4];
    uid7[4] = nfc_xbuf[5];
    uid7[5] = nfc_xbuf[6];
    uid7[6] = nfc_xbuf[7];
    return (nfc_xbuf[0] == 0x04) ? 1 : 0;
}

uint8_t nfc_read_label(uint8_t *lbl4) {
    if (!nfc_read_block(0x0C, nfc_xbuf)) return 0;
    lbl4[0] = nfc_xbuf[15];
    lbl4[1] = nfc_xbuf[14];
    lbl4[2] = nfc_xbuf[13];
    lbl4[3] = nfc_xbuf[12];
    return 1;
}

/* ---- hex nibble helper ---- */
static uint8_t hexch(uint8_t v) {
    return (v < 10) ? ('0' + v) : ('A' + v - 10);
}

void nfc_write_ndef_url(const uint8_t *label4) {
    static const __code uint8_t prefix[] = "pricetag-ap.local/tag/";
    uint8_t i, p;

    /* Block 1: TLV header + NDEF header + start of URI */
    for (i = 0; i < 16; i++) nfc_xbuf[i] = 0;
    p = 0;
    nfc_xbuf[p++] = 0x03;        /* NDEF TLV type */
    nfc_xbuf[p++] = 35;          /* TLV length: 4 + 1 + 22 + 8 = 35 */
    nfc_xbuf[p++] = 0xD1;        /* MB=1 ME=1 SR=1 TNF=1 */
    nfc_xbuf[p++] = 0x01;        /* type length */
    nfc_xbuf[p++] = 31;          /* payload length: 1 + 22 + 8 */
    nfc_xbuf[p++] = 0x55;        /* type 'U' */
    nfc_xbuf[p++] = 0x03;        /* http:// */
    for (i = 0; i < 9 && p < 16; i++) nfc_xbuf[p++] = prefix[i];
    nfc_write_block(1, nfc_xbuf);

    /* Block 2: rest of prefix + hex label */
    for (i = 0; i < 16; i++) nfc_xbuf[i] = 0;
    p = 0;
    for (i = 9; i < 22; i++) nfc_xbuf[p++] = prefix[i];
    nfc_xbuf[p++] = hexch(label4[0] >> 4);
    nfc_xbuf[p++] = hexch(label4[0] & 0x0F);
    nfc_xbuf[p++] = hexch(label4[1] >> 4);
    nfc_write_block(2, nfc_xbuf);

    /* Block 3: rest of hex label + terminator */
    for (i = 0; i < 16; i++) nfc_xbuf[i] = 0;
    p = 0;
    nfc_xbuf[p++] = hexch(label4[1] & 0x0F);
    nfc_xbuf[p++] = hexch(label4[2] >> 4);
    nfc_xbuf[p++] = hexch(label4[2] & 0x0F);
    nfc_xbuf[p++] = hexch(label4[3] >> 4);
    nfc_xbuf[p++] = hexch(label4[3] & 0x0F);
    nfc_xbuf[p++] = 0xFE;        /* terminator TLV */
    nfc_write_block(3, nfc_xbuf);
}

void nfc_write_status(uint8_t rssi, uint8_t lqi, uint16_t polls, uint8_t misses) {
    uint8_t i;
    for (i = 0; i < 16; i++) nfc_xbuf[i] = 0;
    nfc_xbuf[0] = 0x53; nfc_xbuf[1] = 0x54;
    nfc_xbuf[2] = rssi;
    nfc_xbuf[3] = lqi;
    nfc_xbuf[4] = (uint8_t)(polls >> 8);
    nfc_xbuf[5] = (uint8_t)(polls & 0xFF);
    nfc_xbuf[6] = misses;
    nfc_write_block(NFC_STATUS_BLOCK, nfc_xbuf);
}

uint8_t nfc_read_config(uint16_t *poll_s, uint8_t *channel, uint8_t *flags) {
    if (!nfc_read_block(NFC_CFG_BLOCK, nfc_xbuf)) return 0;
    if (nfc_xbuf[0] != NFC_MAGIC0 || nfc_xbuf[1] != NFC_MAGIC1) return 0;
    *poll_s  = ((uint16_t)nfc_xbuf[2] << 8) | nfc_xbuf[3];
    *channel = nfc_xbuf[4];
    *flags   = nfc_xbuf[5];
    return 1;
}
