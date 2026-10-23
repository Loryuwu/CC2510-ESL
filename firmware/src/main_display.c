/*
 * PriceTag CC2510 - Display firmware (enrollment + full-frame image node)
 *
 * GENERIC and FINAL: the gateway renders the whole image and streams it here
 * RLE-compressed; this firmware never needs changing for new content.
 *
 *   1. Read identity from NFC (7-byte UID + 4-byte printed label id).
 *   2. Enroll with the gateway on address 0x00; the gateway owns the
 *      UID->address registry and assigns our short address.
 *   3. Serve image streams addressed to our assigned address.
 *   4. Watchdog: if the gateway goes silent for a long time (e.g. it
 *      rebooted), drop back to enrollment so we get re-assigned.
 *
 * Protocol (payload from radio_rx):
 *   CMD_IMG_BEGIN 0x20            -> wake panel, start B/W plane; ACK
 *   CMD_IMG_DATA  0x21 [rle...]   -> feed RLE token bytes to the panel; ACK
 *   CMD_IMG_SHOW  0x22            -> refresh + deep sleep; ACK
 *   CMD_PING      0xF0            -> ACK
 *   CMD_ACK       0xF1 [status]   (reply we send)
 * Enrollment:
 *   ENROLL_REQ     0xE0 [uid0..6][lbl0..3]   we -> gateway (src 0x00)
 *   ENROLL_ASSIGN  0xE1 [uid0..6][addr]      gateway -> us (dst 0x00)
 *   ENROLL_PENDING 0xE2 [uid0..6]            gateway -> us (dst 0x00)
 *
 * Clock: the EPD's delays are tuned for the power-on RC oscillator while the
 * radio needs the 26 MHz crystal, so panel-timing-critical steps (wake,
 * refresh) run on RC and everything else on the crystal.
 */

#include <cc2510fx.h>
#include "epd.h"
#include "radio.h"
#include "nfc.h"

extern void delay_ms(uint16_t ms);

/* ---- RGB LEDs ----
 * The RGB LEDs (R=P1.5 G=P1.7 B=P1.6) are powered by the boost converter on
 * P2.2. They were used for bring-up diagnostics; the link works now, so they
 * stay OFF for power (keeping the boost off also saves current, important once
 * the tag sleeps). The LED_x writes elsewhere are harmless no-ops with the
 * boost disabled. Set LED_DEBUG to 1 to bring them back for diagnostics.
 * NOTE: P2.2 is also the debug clock. */
#define LED_DEBUG 0
#define BV(x)     (1 << (x))
#define LED_BOOST P2_2
#define LED_R     P1_5
#define LED_G     P1_7
#define LED_B     P1_6

static void led_init(void) {
    P2DIR |= BV(2);
    LED_BOOST = 0;          /* boost OFF -> LEDs cannot light (off) */
#if LED_DEBUG
    P1DIR |= BV(5) | BV(6) | BV(7);
    LED_BOOST = 1;
    LED_R = 0; LED_G = 0; LED_B = 0;
#endif
}

#define ENROLL_ADDR        0x00
#define CMD_IMG_BEGIN      0x20
#define CMD_IMG_DATA       0x21
#define CMD_IMG_SHOW       0x22
#define CMD_POLL           0x23   /* display -> gateway: awake, anything new? */
#define CMD_NOUPDATE       0x24   /* gateway -> display: nothing new, sleep   */
#define CMD_ACK            0xF1
#define CMD_ENROLL_REQ     0xE0
#define CMD_ENROLL_ASSIGN  0xE1
#define CMD_ENROLL_PENDING 0xE2

/* Pull model: the tag sleeps, wakes every POLL_SECONDS, polls the gateway and
 * takes an image if one is offered, then sleeps again. The e-ink holds its
 * picture with zero power, so the tag is off ~99% of the time. */
#define POLL_SECONDS    300        /* default 5 min; overridable by the gateway */
#define POLL_MISS_LIMIT 4          /* silent polls before we try re-enrolling */
#define HOME_CHANNEL    76         /* 2476 MHz: fixed rendezvous + fallback ch */

/* poll_once() outcomes */
#define POLL_SILENT  0             /* gateway did not answer at all */
#define POLL_NONE    1             /* gateway answered "nothing new" */
#define POLL_UPDATED 2             /* an image was streamed */

static __xdata uint8_t uid[7];
static __xdata uint8_t label[4];
static __xdata uint8_t txb[16];
static __xdata uint8_t rxb[RADIO_MAX_PAYLOAD];

static __xdata char line_id[20];
static __xdata char line_status[24];

static __xdata uint16_t g_poll_count = 0;
static __xdata uint8_t  g_ndef_written = 0;
static __xdata uint8_t  g_has_image = 0;
static __xdata uint8_t  g_offline_shown = 0;

/* Image-stream de-duplication: the gateway tags every reliable packet with a
 * monotonic seq. Because the link is stop-and-wait, a lost ACK makes the
 * gateway retransmit the SAME packet; without dedup the CC would feed those
 * bytes into the stateful RLE decoder twice and desync it for the rest of the
 * frame (-> snow). We process each seq exactly once and just re-ACK repeats. */
static __xdata uint8_t last_rx_seq;
static __xdata uint8_t seq_valid;

/* Host-controlled config, carried in the gateway's poll replies (NOUPDATE /
 * IMG_SHOW). Defaults = the old hard-coded behaviour, so an un-upgraded or
 * silent gateway changes nothing. g_next_channel is applied just before sleep
 * so the next poll lands on it; the gateway switches at the same moment. */
static __xdata uint16_t g_poll_secs   = POLL_SECONDS;
static __xdata uint8_t  g_cur_channel = HOME_CHANNEL;
static __xdata uint8_t  g_next_channel = HOME_CHANNEL;

/* sleep = [hi][lo] seconds (clamped sane), ch = nRF24 channel (1..125). */
static void apply_cfg(uint8_t sh, uint8_t sl, uint8_t ch) {
    uint16_t s = ((uint16_t)sh << 8) | sl;
    if (s >= 10 && s <= 65000) g_poll_secs = s;
    if (ch >= 1 && ch <= 125)  g_next_channel = ch;
}

static const __code char HEX[] = "0123456789ABCDEF";

/*
 * The radio needs the 26 MHz crystal; the EPD's delays are tuned for the
 * power-on RC oscillator AND the panel must be driven at that timing. So we
 * run EPD work on RC and everything else on XOSC, switching around each
 * physical panel access. CLKCON.OSC: 1 = RC, 0 = crystal.
 */
#define CLKCON_OSC 0x40
static void clk_rc(void) {
    uint16_t t = 60000;
    CLKCON |= CLKCON_OSC;
    while (!(CLKCON & CLKCON_OSC) && --t) { }
}
static void clk_xosc(void) {
    uint16_t t = 60000;
    CLKCON &= (uint8_t)~CLKCON_OSC;
    while ((CLKCON & CLKCON_OSC) && --t) { }
}

/*
 * Sleep-timer interrupt: its only job is to wake the CPU out of PM2. Must
 * exist (vector ST_VECTOR) and clear the event flag.
 */
void st_isr(void) __interrupt(ST_VECTOR) {
    STIF = 0;
    WORIRQ &= (uint8_t)~0x01;   /* clear EVENT0_FLAG */
}

/*
 * Enter PM2 for ~`secs` seconds, woken by the sleep timer (Event0). PM2 keeps
 * RAM + all SFRs (so the radio/EPD config survive) and runs only the 32kHz RC
 * + sleep timer, dropping the tag to microamps. WOR_RES=3 => 1s per tick, so
 * the 16-bit Event0 compare = seconds directly.
 *
 * ISOLATED ON PURPOSE: the PM2 entry sequence is timing-sensitive on the
 * CC2510. If the tag ever fails to wake, this function is the only place to
 * adjust (and re-flashing recovers it).
 */
static void cc_sleep(uint16_t secs) {
    volatile uint8_t w;

    WORCTRL = 0x03;              /* WOR_RES = 3  -> 1 s per tick */
    WORCTRL |= 0x04;             /* WOR_RESET: Event0 timer restarts from 0 */
    for (w = 20; w; w--) { }     /* let the 32kHz domain settle after reset */

    WOREVT1 = (uint8_t)(secs >> 8);
    WOREVT0 = (uint8_t)(secs & 0xFF);

    WORIRQ |= 0x10;              /* EVENT0_MASK: enable the Event0 interrupt */
    STIF = 0;                    /* clear sleep-timer flag */
    STIE = 1;                    /* enable sleep-timer interrupt (IEN0.5) */
    EA   = 1;                    /* global interrupts on so Event0 can wake us */

    SLEEP = (uint8_t)((SLEEP & 0xFC) | 0x02);   /* select PM2 */
    __asm
      nop
      nop
      nop
    __endasm;
    if (SLEEP & 0x03) {
        PCON |= 0x01;            /* actually enter the power mode */
        __asm nop __endasm;
    }
    /* ---- woken by Event0 (st_isr ran); resume here ---- */
    EA   = 0;
    STIE = 0;
    WORIRQ &= (uint8_t)~0x10;
}

/*
 * Push the current draw list to the panel: wake, refresh, deep-sleep. Deep
 * sleep between updates is what prevents the red smear. Draw-list building
 * (epd_clear/rect/text) is RAM-only and safe to do while asleep / on XOSC.
 */
static void epd_render(void) {
    clk_rc();
    epd_wake();
    epd_update();
    epd_sleep();
    clk_xosc();
}

static void put_hex(char *dst, uint8_t v) {
    dst[0] = HEX[v >> 4];
    dst[1] = HEX[v & 0x0F];
}

static void build_id_line(void) {
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
    epd_render();
}

static void show_offline(void) {
    epd_clear(WHITE);
    epd_rect(0, 0, SCR_W, 20, RED);
    epd_text(8, 4, "OFFLINE", WHITE, 2);
    epd_text(10, 30, line_id, BLACK, 1);
    epd_text(10, 44, "Suche Gateway...", BLACK, 1);
    epd_render();
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
    txb[n++] = PANEL_TYPE;
    radio_tx(txb, n);
}

static void send_ack(uint8_t status, uint8_t seq) {
    /* Turnaround delay: the gateway is still switching nRF24 from TX to RX
     * right after it sent us the command. If we ACK immediately it arrives
     * before the gateway is listening and is lost every time (a systematic
     * race, not packet loss). Wait so the gateway is in RX, then ACK twice.
     * The ACK echoes the packet's seq so the gateway only advances when THIS
     * packet was acked (a stale ACK from the previous packet no longer counts,
     * which used to let the gateway skip ahead and desync the stream). */
    txb[0] = CMD_ACK;
    txb[1] = status;
    txb[2] = seq;
    txb[3] = radio_rssi;      /* link metrics of the packet we're acking, so */
    txb[4] = radio_lqi;       /* the gateway can print why the CC->path is   */
    txb[5] = radio_freqest;   /* noisy (signal vs demod vs frequency).       */
    delay_ms(10);
    radio_tx(txb, 6);
    radio_tx(txb, 6);
}

/*
 * Enroll on 0x00: try `max_attempts` times (each ~600ms), return the assigned
 * address or 0 if the gateway didn't respond. The caller decides whether to
 * sleep and retry, so the tag doesn't burn battery in a busy loop. Draws
 * NOTHING on screen, keeping the last weather image intact.
 */
#define ENROLL_ATTEMPTS 10

static uint8_t do_enroll(void) {
    uint8_t len, attempt;
    uint8_t shown_wait = 0;

    radio_set_addr(ENROLL_ADDR);

    for (attempt = 0; attempt < ENROLL_ATTEMPTS; attempt++) {
        LED_R = 1;
        send_enroll_req();
        LED_R = 0;
        if (radio_rx(rxb, &len, 600)) {
            if (rxb[0] == CMD_ENROLL_ASSIGN && len >= 9 && uid_matches(&rxb[1])) {
                radio_set_addr(rxb[8]);
                LED_G = 1; delay_ms(80); LED_G = 0; delay_ms(80);
                LED_G = 1; delay_ms(80); LED_G = 0;
                return rxb[8];
            } else if (rxb[0] == CMD_ENROLL_PENDING && len >= 8
                       && uid_matches(&rxb[1]) && !shown_wait) {
                show("Warte Freigabe");
                shown_wait = 1;
                attempt = 0;   /* keep trying while gateway says "pending" */
            }
        }
    }
    return 0;
}

/* Process one received image-stream packet (BEGIN/DATA/SHOW) with seq dedup
 * and ACK it. Returns 1 if it was SHOW (end of stream). */
static uint8_t handle_img(uint8_t len) {
    uint8_t seq = rxb[1];

    switch (rxb[0]) {
    case CMD_IMG_BEGIN:
        if (!seq_valid || seq != last_rx_seq) {
            /* ACK first, THEN wake the panel (~1.2s): a short gateway BEGIN
             * timeout can then retry fast on a weak link instead of waiting
             * seconds. The gateway only sends DATA after this ACK; the first
             * DATA packets land while we are still in epd_img_begin and get
             * retransmitted (DATA has a big retry budget), so none are lost. */
            last_rx_seq = seq; seq_valid = 1;
            send_ack(0, seq);
            clk_rc(); epd_img_begin(); clk_xosc();
        } else {
            send_ack(0, seq);
        }
        return 0;

    case CMD_IMG_DATA:
        if (!seq_valid || seq != last_rx_seq) {
            if (len > 2) epd_img_rle(&rxb[2], (uint8_t)(len - 2));
            last_rx_seq = seq; seq_valid = 1;
        }
        send_ack(0, seq);
        return 0;

    case CMD_IMG_SHOW:
        if (!seq_valid || seq != last_rx_seq) {
            clk_rc();
            if (epd_img_complete()) epd_img_show();  /* refresh + deep sleep */
            else                    epd_sleep();     /* truncated -> keep image */
            clk_xosc();
            last_rx_seq = seq; seq_valid = 1;
        }
        send_ack(0, seq);
        /* IMG_SHOW may carry [seq][sleep_hi][sleep_lo][channel] for next cycle */
        if (len >= 5) apply_cfg(rxb[2], rxb[3], rxb[4]);
        return 1;
    }
    return 0;
}

/*
 * Poll the gateway once: "I'm awake, got anything new?". If it streams an
 * image we receive it to the end; if it says NOUPDATE (or we were offered
 * nothing) we return so the caller can sleep again.
 *   POLL_SILENT  - no answer (gateway may be gone / lost our registration)
 *   POLL_NONE    - gateway answered, nothing new
 *   POLL_UPDATED - an image was streamed
 */
/* After POLL, keep listening this many silent ~2s windows (~7s) before
 * re-polling - long enough to outlast both the gateway's BEGIN retransmit gap
 * (~3s) and its per-DATA-packet retry budget (~2.4s), so a briefly lost packet
 * at range is still caught instead of us bailing and sleeping too early. */
#define POLL_STALL_LIMIT 4
/* Give up (sleep) only after this many re-polls produced nothing at all. */
#define POLL_RETRY_LIMIT 4

static uint8_t poll_once(void) {
    uint8_t len, stalls, polls;

    seq_valid = 0;
    stalls = 0;
    polls = 0;

    /* POLL carries the CC's downlink diagnostics: RSSI of the last burst it
     * synced on, and the running "synced-but-bad" counter. Lets the gateway
     * print how strongly its signal actually reaches the CC. */
    txb[0] = CMD_POLL;
    txb[1] = radio_rssi;
    txb[2] = radio_rx_bad;
    radio_tx(txb, 3);

    for (;;) {
        if (radio_rx(rxb, &len, 2000)) {
            stalls = 0;
            if (len >= 1) {
                if (rxb[0] == CMD_NOUPDATE) {
                    /* NOUPDATE may carry [sleep_hi][sleep_lo][channel] */
                    if (len >= 4) apply_cfg(rxb[1], rxb[2], rxb[3]);
                    return POLL_NONE;
                }
                if (handle_img(len))       /* 1 on SHOW -> stream complete */
                    return POLL_UPDATED;
            }
            continue;
        }
        /* Silence. Stay on air across the gateway's retransmits; only re-poll
         * after a run of silence (the first POLL or BEGIN may have been lost),
         * and only give up for this cycle after several fruitless re-polls. */
        if (++stalls >= POLL_STALL_LIMIT) {
            if (++polls >= POLL_RETRY_LIMIT)
                return POLL_SILENT;
            txb[0] = CMD_POLL;
            txb[1] = radio_rssi;
            txb[2] = radio_rx_bad;
            radio_tx(txb, 3);
            stalls = 0;
        }
    }
}

void main(void) {
    uint8_t misses;

    led_init();                /* LEDs off (boost disabled) */

    radio_init(ENROLL_ADDR);   /* selects the crystal (XOSC) */
    nfc_init();

    if (!nfc_read_uid(uid)) {
        epd_clear(WHITE);
        epd_text(10, 40, "NFC Fehler", RED, 3);
        epd_render();
        for (;;) { }
    }
    nfc_read_label(label);
    build_id_line();

    show("Verbinde...");

    for (;;) {
        uint8_t addr;

        g_cur_channel = HOME_CHANNEL; g_next_channel = HOME_CHANNEL;
        radio_set_channel(HOME_CHANNEL);

        addr = do_enroll();
        if (addr == 0) {
            /* Gateway nicht erreichbar. Einmal Offline-Marker zeigen (nur wenn
             * schon ein Wetterbild da war), dann schlafen statt busy-wait. */
            if (g_has_image && !g_offline_shown) {
                show_offline();
                g_offline_shown = 1;
            }
            radio_sleep();
            cc_sleep(30);
            radio_wake();
            continue;
        }

        g_offline_shown = 0;
        if (!g_ndef_written) { nfc_write_ndef_url(label); g_ndef_written = 1; }

        misses = 0;
        for (;;) {
            uint8_t result = poll_once();
            g_poll_count++;
            if (result == POLL_UPDATED) g_has_image = 1;
            if (result == POLL_SILENT) {
                if (++misses >= POLL_MISS_LIMIT)
                    break;
            } else {
                misses = 0;
            }
            if (g_next_channel != g_cur_channel) {
                radio_set_channel(g_next_channel);
                g_cur_channel = g_next_channel;
            }

            nfc_write_status(radio_rssi, radio_lqi, g_poll_count, misses);

            { uint16_t cfg_poll; uint8_t cfg_ch, cfg_flags;
              if (nfc_read_config(&cfg_poll, &cfg_ch, &cfg_flags)) {
                  if (cfg_poll >= 10 && cfg_poll <= 65000) g_poll_secs = cfg_poll;
                  if (cfg_ch >= 1 && cfg_ch <= 125) g_next_channel = cfg_ch;
              }
            }

            radio_sleep();
            cc_sleep(g_poll_secs);
            radio_wake();
        }
    }
}
