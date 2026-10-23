/*
 * PriceTag CC2510 - NFC memory dump (find the label ID + map NDEF layout)
 *
 * Reads I2C blocks 0..15 (256 bytes, pages 0..63) of the NTAG I2C plus and
 * reports each block over radio. We're hunting for the printed label ID
 * (e.g. "D0DD52AC") to decide whether it can serve as the human-visible
 * device identity, and mapping where the NDEF message lives so our config
 * block doesn't clobber it.
 *
 * Read via gateway_rx_test raw hex. One packet per block:
 *   [0xC2][block][16 bytes]
 *
 * Look in the 16 payload bytes for:
 *   - binary  D0 DD 52 AC
 *   - ASCII   44 30 44 44 35 32 41 43  ("D0DD52AC", e.g. inside a URL)
 *   - printable NDEF text (0x20..0x7E) such as "http" = 68 74 74 70
 *
 * Read-only: never writes. (Block 1 may already hold the 'PT' test config
 * from main_nfc_test - that's fine, we're just looking.)
 */

#include <cc2510fx.h>
#include "radio.h"
#include "nfc.h"

extern void delay_ms(uint16_t ms);

#define FIRST_BLOCK 0
#define LAST_BLOCK  15

static __xdata uint8_t blk[16];
static __xdata uint8_t rep[20];

void main(void) {
    uint8_t b, i, ok;

    radio_init(0x01);
    nfc_init();

    for (;;) {
        for (b = FIRST_BLOCK; b <= LAST_BLOCK; b++) {
            ok = nfc_read_block(b, blk);

            rep[0] = 0xC2;
            rep[1] = b;
            for (i = 0; i < 16; i++) rep[2 + i] = ok ? blk[i] : 0x00;
            radio_tx(rep, 18);

            delay_ms(150);
        }
        delay_ms(1500);
    }
}
