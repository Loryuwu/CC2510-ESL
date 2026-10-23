/*
 * PriceTag CC2510 - Radio RX Loopback Diagnostic
 *
 * Purpose: make the invisible ESP->CC direction VISIBLE.
 *
 * The CC->ESP direction is proven to work (nRF24 receives clean packets).
 * So we exploit it: whatever the CC2510 receiver captures, it echoes back
 * to the ESP over the working path. The ESP serial then shows the EXACT
 * raw bytes the CC2510 demodulator delivered - the ground truth we have
 * never been able to see before.
 *
 * Loop:
 *   - Listen for a raw burst (radio_rx_raw: no filter, no decode).
 *   - If something was captured -> echo it back, prefixed with 0xE0.
 *   - If nothing was captured  -> send a heartbeat, prefixed with 0xBB.
 *
 * ESP serial interpretation:
 *   - Only heartbeats (0xBB)  => CC alive + CC->ESP works, but RX never
 *                                achieves sync lock  -> PHY-layer problem.
 *   - Echo packets (0xE0 ...) => sync lock works! The bytes after 0xE0 are
 *                                the raw OTA capture. Expect the first byte
 *                                to be 0xA7 (nRF24 addr byte 3), then the
 *                                protocol payload. This reveals the correct
 *                                byte offset / bit alignment directly.
 *
 * No LEDs / no boost here on purpose: P2.1/P2.2 are shared with the debug
 * DD/DC lines, so this firmware works whether or not the debugger stays
 * wired. All diagnosis happens on the ESP serial.
 */

#include <cc2510fx.h>
#include "radio.h"

extern void delay_ms(uint16_t ms);
extern uint8_t radio_dbg_marc;
extern uint8_t radio_dbg_pktst;

#define RAW_CAP   24          /* bytes to capture/echo (<= RADIO_MAX_PAYLOAD-3) */

static __xdata uint8_t raw[RAW_CAP];
static __xdata uint8_t out[RAW_CAP + 3];

void main(void) {
    uint8_t got, i;
    uint16_t seq = 0;

    radio_init(0x01);

    /* If the radio never reached IDLE, there is nothing we can do - just
     * spin. (No LED; the ESP will simply never see any packet.) */
    if ((MARCSTATE & 0x1F) != 0x01) {
        while (1) { }
    }

    while (1) {
        got = radio_rx_raw(raw, RAW_CAP, 300);   /* ~300ms listen window */

        if (got > 0) {
            /* Echo: 0xE0, got, stall-MARCSTATE, stall-PKTSTATUS, raw... */
            out[0] = 0xE0;
            out[1] = got;
            out[2] = radio_dbg_marc;
            out[3] = radio_dbg_pktst;
            for (i = 0; i < got; i++)
                out[4 + i] = raw[i];
            radio_tx(out, (uint8_t)(got + 4));
        } else {
            /* Heartbeat: marker 0xBB + 2-byte seq (proves CC alive + TX ok) */
            out[0] = 0xBB;
            out[1] = (uint8_t)(seq >> 8);
            out[2] = (uint8_t)(seq & 0xFF);
            radio_tx(out, 3);
            seq++;
        }
    }
}
