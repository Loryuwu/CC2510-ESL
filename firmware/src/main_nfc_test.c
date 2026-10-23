/*
 * PriceTag CC2510 - NFC driver test (read UID + EEPROM write/read-back)
 *
 * Proves the NTAG I2C plus storage mechanism that Milestone C (enrollment)
 * depends on: read the factory UID (identity) and write/read-back the
 * assigned address in user block 1 (survives firmware re-flash).
 *
 * Writes ONCE (no EEPROM wear), then loops reporting over radio. Read via
 * gateway_rx_test raw hex:
 *   [0xC0][ok][uid0..6]                    UID packet (9 bytes)
 *   [0xC1][ok][match][cfg0..15]            config block read-back (19 bytes)
 *     ok    = chip ACKed
 *     match = 1 if read-back equals what we wrote (magic 'P''T' + addr)
 *
 * WARNING: writes only to user block 1 (safe user memory). Never touches
 * block 0 (UID) or the config/session registers.
 */

#include <cc2510fx.h>
#include "radio.h"
#include "nfc.h"

extern void delay_ms(uint16_t ms);

#define TEST_ADDR 0x2A     /* pretend this is the assigned short address */

static __xdata uint8_t uid[7];
static __xdata uint8_t cfg[16];
static __xdata uint8_t rdbk[16];
static __xdata uint8_t rep[20];

void main(void) {
    uint8_t uid_ok, rd_ok, match, i;

    radio_init(0x01);
    nfc_init();

    uid_ok = nfc_read_uid(uid);

    /* Build and write our config block once. */
    for (i = 0; i < 16; i++) cfg[i] = 0xFF;
    cfg[0] = NFC_MAGIC0;      /* 'P' */
    cfg[1] = NFC_MAGIC1;      /* 'T' */
    cfg[2] = TEST_ADDR;
    nfc_write_block(NFC_CFG_BLOCK, cfg);

    for (;;) {
        /* --- UID packet --- */
        rep[0] = 0xC0;
        rep[1] = uid_ok;
        for (i = 0; i < 7; i++) rep[2 + i] = uid[i];
        radio_tx(rep, 9);

        delay_ms(60);

        /* --- config read-back packet --- */
        rd_ok = nfc_read_block(NFC_CFG_BLOCK, rdbk);
        match = (rd_ok && rdbk[0] == NFC_MAGIC0 && rdbk[1] == NFC_MAGIC1
                 && rdbk[2] == TEST_ADDR) ? 1 : 0;
        rep[0] = 0xC1;
        rep[1] = rd_ok;
        rep[2] = match;
        for (i = 0; i < 16; i++) rep[3 + i] = rdbk[i];
        radio_tx(rep, 19);

        delay_ms(1000);
    }
}
