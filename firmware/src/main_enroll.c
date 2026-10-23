/*
 * PriceTag CC2510 - Milestone C: enrollment client (stateless display)
 *
 * On boot the display reads its permanent identity from the NFC chip
 * (7-byte UID + 4-byte printed label id), then registers with the gateway.
 * It holds NO persistent address of its own - the gateway owns the
 * UID->address registry, so this survives firmware re-flash automatically.
 *
 * Flow:
 *   my_addr = 0x00 (enroll channel)
 *   loop: send ENROLL_REQ{uid,label}; listen briefly
 *     ENROLL_ASSIGN{uid,addr} (uid matches us) -> my_addr=addr, go READY
 *     ENROLL_PENDING{uid}                      -> show "waiting for approval"
 *   READY: serve the radio (here: answer PING; full draw loop is main_display)
 *
 * The EPD shows live status so you can see enrollment happen on the panel.
 *
 * Protocol (payload from radio_rx):
 *   ENROLL_REQ     0xE0 [uid0..6][lbl0..3]      display -> gateway (src 0x00)
 *   ENROLL_ASSIGN  0xE1 [uid0..6][addr]         gateway -> display (dst 0x00)
 *   ENROLL_PENDING 0xE2 [uid0..6]               gateway -> display (dst 0x00)
 *   PING 0xF0 / ACK 0xF1 as before
 */

#include <cc2510fx.h>
#include "epd.h"
#include "radio.h"
#include "nfc.h"

extern void delay_ms(uint16_t ms);

#define ENROLL_ADDR        0x00
#define CMD_ENROLL_REQ     0xE0
#define CMD_ENROLL_ASSIGN  0xE1
#define CMD_ENROLL_PENDING 0xE2
#define CMD_PING           0xF0
#define CMD_ACK            0xF1

static __xdata uint8_t uid[7];
static __xdata uint8_t label[4];
static __xdata uint8_t txb[16];
static __xdata uint8_t rxb[RADIO_MAX_PAYLOAD];

/* persistent strings for epd_text (it keeps pointers until epd_update) */
static __xdata char line_id[20];
static __xdata char line_status[24];

static const __code char HEX[] = "0123456789ABCDEF";

static void put_hex(char *dst, uint8_t v) {
    dst[0] = HEX[v >> 4];
    dst[1] = HEX[v & 0x0F];
}

static void build_id_line(void) {
    /* "ID B0DD52AC" */
    uint8_t i, p = 0;
    line_id[p++] = 'I'; line_id[p++] = 'D'; line_id[p++] = ' ';
    for (i = 0; i < 4; i++) { put_hex(&line_id[p], label[i]); p += 2; }
    line_id[p] = 0;
}

static void show(const char *status) {
    uint8_t i;
    for (i = 0; status[i] && i < 23; i++) line_status[i] = status[i];
    line_status[i] = 0;

    epd_clear(WHITE);
    epd_rect(0, 0, SCR_W, 24, RED);
    epd_text(70, 6, "HA DISPLAY", WHITE, 2);
    epd_text(10, 40, line_id, BLACK, 2);
    epd_text(10, 72, line_status, BLACK, 2);
    epd_update();
}

static uint8_t uid_matches(const uint8_t *p) {
    uint8_t i;
    for (i = 0; i < 7; i++) if (p[i] != uid[i]) return 0;
    return 1;
}

static void send_enroll_req(void) {
    uint8_t i, n = 0;
    txb[n++] = CMD_ENROLL_REQ;
    for (i = 0; i < 7; i++) txb[n++] = uid[i];
    for (i = 0; i < 4; i++) txb[n++] = label[i];
    radio_tx(txb, n);
}

static void send_ack(uint8_t status) {
    txb[0] = CMD_ACK;
    txb[1] = status;
    radio_tx(txb, 2);
}

void main(void) {
    uint8_t len, assigned, my = 0;

    /* EPD first (RC clock), then radio (XOSC), then NFC. */
    epd_init();

    radio_init(ENROLL_ADDR);
    nfc_init();

    if (!nfc_read_uid(uid)) {
        /* No NFC identity -> cannot enroll. Show error and stop. */
        epd_clear(WHITE);
        epd_text(10, 40, "NFC Fehler", RED, 3);
        epd_update();
        for (;;) { }
    }
    nfc_read_label(label);     /* best-effort human label */
    build_id_line();

    show("Suche Gateway");

    assigned = 0;
    while (!assigned) {
        send_enroll_req();
        if (radio_rx(rxb, &len, 500)) {
            if (rxb[0] == CMD_ENROLL_ASSIGN && len >= 9 && uid_matches(&rxb[1])) {
                my = rxb[8];
                radio_set_addr(my);
                assigned = 1;
            } else if (rxb[0] == CMD_ENROLL_PENDING && len >= 8 && uid_matches(&rxb[1])) {
                show("Warte Freigabe");
            }
        }
        delay_ms(400);
    }

    /* Registered: show assigned address, then serve. */
    {
        char s[20];
        uint8_t p = 0;
        s[p++] = 'A'; s[p++] = 'd'; s[p++] = 'r'; s[p++] = ' ';
        put_hex(&s[p], my); p += 2;
        s[p++] = ' '; s[p++] = 'O'; s[p++] = 'K'; s[p] = 0;
        show(s);
    }

    for (;;) {
        if (!radio_rx(rxb, &len, 2000)) continue;
        if (len >= 1 && rxb[0] == CMD_PING)
            send_ack(0);
        /* draw commands are handled by the full display firmware */
    }
}
