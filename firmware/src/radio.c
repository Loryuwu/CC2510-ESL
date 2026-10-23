#include <cc2510fx.h>
#include "radio.h"

extern void delay_ms(uint16_t ms);

/* RFST strobe commands */
#define STROBE_IDLE  0x04
#define STROBE_RX    0x02
#define STROBE_TX    0x03

#define MARC_IDLE  0x01

/* CLKCON.OSC: 0 = use the 24-27 MHz crystal, 1 = use HS RC oscillator. */
#define CLKCON_OSC 0x40

/* A non-repeating, byte-palindromic nRF24 address avoids false sync locks. */
#define NRF_ADDR1     0xA7
#define NRF_ADDR2     0x5C
#define NRF_ADDR3     0xA7

/*
 * nRF24 runs with CRC disabled -> plain ShockBurst, NO 9-bit PCF.
 * On air after CC2510 sync word (A7 5C):
 *   [ADDR3 = 0xA7 completes the nRF24 3-byte address][payload...]
 * The nRF24 matches A7 5C A7 and delivers everything after it as
 * payload, byte-aligned, with no PCF. So no bit-shifting is needed.
 */
#define OTA_SIZE         35

static uint8_t my_addr;
static uint8_t radio_ready;

/* Diagnostic: MARCSTATE captured at the moment radio_rx_raw stalls, plus
 * the PKTSTATUS snapshot. Lets the loopback report WHY reception stops. */
__xdata uint8_t radio_dbg_marc;
__xdata uint8_t radio_dbg_pktst;

/* Incremented whenever a frame synced and was captured but then failed the
 * address/length/CRC checks (i.e. corrupt or not-for-us) - as opposed to a
 * plain timeout where nothing arrived. Lets the caller flash a "bad packet"
 * indicator distinct from "idle". In XDATA to spare the tight internal RAM. */
__xdata uint8_t radio_rx_bad;

/* Link metrics captured on the last received burst, so the quality of the
 * gateway->CC path can be reported back (via the ACK) and we can tell signal
 * (RSSI) from demod quality (LQI) from frequency offset (FREQEST). */
__xdata uint8_t radio_rssi;
__xdata uint8_t radio_lqi;
__xdata uint8_t radio_freqest;

static uint8_t wait_idle(void) {
    uint16_t t = 60000U;
    while ((MARCSTATE & 0x1F) != MARC_IDLE && --t) {
        __asm nop __endasm;
        __asm nop __endasm;
        __asm nop __endasm;
        __asm nop __endasm;
    }
    return ((MARCSTATE & 0x1F) == MARC_IDLE);
}

/*
 * The RF transceiver cannot operate from the high-speed RC oscillator.
 * Request the 26 MHz crystal and wait until the clock mux confirms it.
 */
static uint8_t select_xosc(void) {
    uint16_t t = 60000U;

    CLKCON &= (uint8_t)~CLKCON_OSC;
    while ((CLKCON & CLKCON_OSC) && --t) {
        __asm nop __endasm;
        __asm nop __endasm;
        __asm nop __endasm;
        __asm nop __endasm;
    }

    return !(CLKCON & CLKCON_OSC);
}

/*
 * CRC-8 (poly 0x07, init 0x00) over the protocol bytes. Replaces the old
 * 8-bit XOR, which let two bit flips in the same column cancel out - those
 * silently-corrupt frames were a source of garbled image streams. Must match
 * the gateway's crc8() byte for byte.
 */
static uint8_t pkt_crc(const uint8_t *buf, uint8_t len) {
    uint8_t crc = 0x00;
    uint8_t b;
    while (len--) {
        crc ^= *buf++;
        for (b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07)
                               : (uint8_t)(crc << 1);
    }
    return crc;
}

void radio_init(uint8_t addr) {
    my_addr = addr;
    radio_ready = select_xosc();
    if (!radio_ready) return;

    RFST = STROBE_IDLE;
    wait_idle();

    /* 250 kbps GFSK, 2476 MHz (nRF24 channel 76) */
    SYNC1    = NRF_ADDR1;
    SYNC0    = NRF_ADDR2;
    PKTLEN   = OTA_SIZE;
    PKTCTRL1 = 0x00;
    PKTCTRL0 = 0x00;      /* fixed len, no CRC, no whitening */
    ADDR     = 0x00;
    CHANNR   = 0x00;

    FSCTRL1  = 0x0A;      /* IF frequency */
    FSCTRL0  = 0x00;

    FREQ2    = 0x5F;      /* 2476 MHz */
    FREQ1    = 0x3B;
    FREQ0    = 0x17;

    MDMCFG4  = 0x0D;      /* RX BW ~812 kHz (max), DRATE_E=13 - tolerate xtal offset */
    MDMCFG3  = 0x3B;      /* 250 kbps */
    MDMCFG2  = 0x11;      /* GFSK, 15/16 sync (no carrier sense - catch weak nRF24 bursts) */
    MDMCFG1  = 0x23;      /* 4 preamble bytes, FEC off */
    MDMCFG0  = 0x11;      /* channel spacing (unused with CHANNR=0) */

    DEVIATN  = 0x65;      /* ±165 kHz (nRF24 uses ±160 kHz) */

    MCSM2    = 0x07;
    MCSM1    = 0x00;      /* RX→IDLE, TX→IDLE */
    MCSM0    = 0x18;      /* auto-cal IDLE→RX/TX */

    FOCCFG   = 0x1F;      /* freq offset comp limit ±BW/2 (was ±BW/8) - tolerate xtal offset */
    BSCFG    = 0x1C;
    AGCCTRL2 = 0xC7;
    AGCCTRL1 = 0x00;
    AGCCTRL0 = 0xB0;

    FREND1   = 0xB6;
    FREND0   = 0x10;      /* PA_POWER index 0 → PA_TABLE0 */

    FSCAL3   = 0xEA;
    FSCAL2   = 0x2A;
    FSCAL1   = 0x00;
    FSCAL0   = 0x1F;

    PA_TABLE0 = 0xFE;     /* ~0 dBm TX power */
}

uint8_t radio_get_marcstate(void) {
    return MARCSTATE & 0x1F;
}

void radio_set_addr(uint8_t addr) {
    my_addr = addr;
}

/*
 * Retune at runtime to nRF24 channel `ch` (carrier 2400+ch MHz).
 *   FREQ = f_carrier * 2^16 / f_xosc,  f_xosc = 26 MHz
 * (+13 for rounding). ch 76 reproduces ~2476 MHz (the compiled home channel,
 * within a few LSB of radio_init's value - the AFC absorbs the <2 kHz diff).
 * Must be in IDLE to change the synthesizer; MCSM0 auto-cal re-calibrates on
 * the next RX/TX strobe.
 */
void radio_set_channel(uint8_t ch) {
    uint32_t f = ((uint32_t)(2400U + ch) * 65536UL + 13UL) / 26UL;
    if (!radio_ready) return;
    RFST = STROBE_IDLE;
    wait_idle();
    FREQ2 = (uint8_t)(f >> 16);
    FREQ1 = (uint8_t)(f >> 8);
    FREQ0 = (uint8_t)(f);
}

static __xdata uint8_t tx_ota[OTA_SIZE];
static __xdata uint8_t tx_proto[RADIO_MAX_PAYLOAD + 4];

uint8_t radio_tx(const uint8_t *buf, uint8_t len) {
    uint8_t plen, i;

    if (!radio_ready || len > RADIO_MAX_PAYLOAD) return 0;

    /* Protocol: [MY_ADDR][LEN][data...][XOR] */
    tx_proto[0] = my_addr;
    tx_proto[1] = len;
    for (i = 0; i < len; i++)
        tx_proto[2 + i] = buf[i];
    plen = len + 2;
    tx_proto[plen] = pkt_crc(tx_proto, plen);
    plen++;

    /*
     * OTA: sync word (A7 5C) is sent by hardware, then:
     *   tx_ota[0] = 0xA7  -> completes the nRF24 3-byte address A7 5C A7
     *   tx_ota[1..] = protocol bytes, delivered to nRF24 as payload
     */
    tx_ota[0] = NRF_ADDR3;
    for (i = 0; i < plen; i++)
        tx_ota[1 + i] = tx_proto[i];
    for (i = 1 + plen; i < OTA_SIZE; i++)
        tx_ota[i] = 0x00;

    /* TX: strobe TX, then stream data via RFTXRXIF flag */
    RFST = STROBE_IDLE;
    if (!wait_idle()) return 0;
    RFST = STROBE_TX;

    for (i = 0; i < OTA_SIZE; i++) {
        while (!(TCON & 0x02)) {}
        TCON &= ~0x02;
        RFD = tx_ota[i];
    }

    return wait_idle();
}

static __xdata uint8_t rx_ota[OTA_SIZE];

/*
 * Receive one protocol frame.
 *
 * On air the gateway sends (after its AA-preamble):
 *   [A7 5C]        CC2510 hardware sync word (consumed, not stored)
 *   [A7]           nRF24 address tail -> rx_ota[0]
 *   [proto...]     [my_addr][len][data...][xor] -> rx_ota[1..]
 *
 * There is NO 9-bit PCF (the gateway disables dynamic payloads), so the
 * stream is byte-aligned: proto byte k lives at rx_ota[1 + k]. The former
 * code assumed a PCF and applied a bogus <<1 shift + wrong offset, which,
 * together with the FIFO-overflow bug (see radio_rx_raw), is why ESP->CC
 * never worked. Verified 2026-10-16 via loopback: capture is exactly
 * "A7 01 06 50 49 4E 47 .." with matching XOR.
 *
 * The FIFO drain itself is done by radio_rx_raw, which stays in lockstep
 * with the ~32us/byte air rate - any delay_ms there overflows the FIFO.
 */
uint8_t radio_rx(uint8_t *buf, uint8_t *len, uint16_t timeout_ms) {
    uint8_t got, plen, i;

    got = radio_rx_raw(rx_ota, OTA_SIZE, timeout_ms);

    /* Smallest valid frame: A7 + addr + len=0 + xor = 4 bytes. got < 4 means
     * no sync / nothing arrived (idle), NOT a corrupt packet - don't count it. */
    if (got < 4)
        return 0;

    /* From here the burst synced and was captured; any rejection below means a
     * corrupt or not-for-us frame, which we flag via radio_rx_bad. */
    if (rx_ota[0] != NRF_ADDR3)
        { radio_rx_bad++; return 0; }

    if (rx_ota[1] != my_addr && rx_ota[1] != RADIO_ADDR_BROADCAST)
        { radio_rx_bad++; return 0; }

    plen = rx_ota[2];
    if (plen > RADIO_MAX_PAYLOAD)
        { radio_rx_bad++; return 0; }

    /* Need A7(0) addr(1) len(2) data(3..) xor -> 4 + plen bytes captured. */
    if (got < (uint8_t)(plen + 4))
        { radio_rx_bad++; return 0; }

    /* CRC over proto[0..plen+1] = rx_ota[1..plen+2], check proto[plen+2]. */
    if (pkt_crc(&rx_ota[1], (uint8_t)(plen + 2)) != rx_ota[plen + 3])
        { radio_rx_bad++; return 0; }

    /*
     * Closed-loop AFC: the CC2510 crystal sits ~tens of kHz off the nRF24, so
     * the demodulator decides bits off-centre -> errors even at huge signal
     * (RSSI -38dBm, LQI 52, FREQEST -28 measured on HW). Nudge the synthesizer
     * offset (FSCTRL0) by the measured error so the next packet is received
     * centred. Only on CRC-GOOD packets (reliable FREQEST), and it accumulates
     * toward residual 0, so it converges in a packet or two and then holds -
     * a bad FREQEST can never drive it because corrupt packets never get here.
     * Clamped to +-50 LSB (~+-80kHz) as a safety net against runaway.
     */
    {
        signed char f = (signed char)((signed char)FSCTRL0 + (signed char)radio_freqest);
        if (f > 50) f = 50;
        else if (f < -50) f = -50;
        FSCTRL0 = (uint8_t)f;
    }

    for (i = 0; i < plen; i++)
        buf[i] = rx_ota[3 + i];
    *len = plen;

    return 1;
}

/*
 * Raw capture for loopback diagnosis. Mirrors radio_rx's PHY path (IDLE ->
 * flush -> RX -> drain RFD per RFTXRXIF) but stops after `n` bytes and does
 * ZERO interpretation. Crucially it only waits for `n` bytes (not the full
 * OTA_SIZE), so a burst shorter than 35 bytes does not cause a trailing
 * timeout that would discard the capture. On per-byte timeout it returns the
 * partial count, so even a truncated sync lock is reported.
 */
uint8_t radio_rx_raw(uint8_t *buf, uint8_t maxlen, uint16_t timeout_ms) {
    uint8_t i, n;
    uint16_t spin;
    uint8_t saved_clk;

    if (!radio_ready) return 0;

    /*
     * Run the CPU at FULL speed (CLKSPD=0, f/1) for the whole capture. The
     * per-byte RX lockstep must read each RFD within its ~32us air slot before
     * the next byte overwrites it; at the reset-default divided clock (e.g.
     * f/2 = 13MHz) the margin is thin and bytes corrupt (distance-independent,
     * TX unaffected since the radio paces TX). Doubling the CPU speed doubles
     * that margin. Restored before every return so the EPD timing (tuned at
     * the divided clock) is untouched.
     */
    saved_clk = CLKCON;
    CLKCON &= (uint8_t)~0x07;

    n = (maxlen < OTA_SIZE) ? maxlen : OTA_SIZE;

    RFST = STROBE_IDLE;
    wait_idle();

    if ((MARCSTATE & 0x1F) == 0x11) { /* RXOVF */
        RFST = STROBE_IDLE;
        wait_idle();
    }

    RFST = STROBE_RX;

    /*
     * RFTXRXIF (TCON.1) is an EVENT per over-air byte, NOT a level "byte in
     * FIFO" flag - exactly like the TX path, where the radio paces the CPU.
     * So we must stay in lockstep from byte 0: read each byte within its
     * ~32us window. ANY latency at the start (e.g. a delay_ms poll) lets the
     * rest of the 1.1ms packet stream past, overflow the FIFO (MARCSTATE
     * 0x11), and kill all further events -> the "got=1 + RXOVF" symptom.
     *
     * First byte: long spin budget to wait for sync. Later bytes: short
     * budget (32us gap << budget), NO delay anywhere.
     */
    {
        uint32_t budget = (uint32_t)timeout_ms * 800UL;
        while (!(TCON & 0x02)) {
            if (--budget == 0) {
                radio_dbg_marc  = MARCSTATE & 0x1F;
                radio_dbg_pktst = PKTSTATUS;
                RFST = STROBE_IDLE;
                CLKCON = saved_clk;
                return 0;            /* no sync lock within timeout */
            }
        }
    }
    TCON &= ~0x02;
    buf[0] = RFD;

    for (i = 1; i < n; i++) {
        spin = 8000;
        while (!(TCON & 0x02)) {
            if (--spin == 0) {
                radio_dbg_marc  = MARCSTATE & 0x1F;
                radio_dbg_pktst = PKTSTATUS;
                radio_rssi = RSSI; radio_lqi = LQI; radio_freqest = FREQEST;
                RFST = STROBE_IDLE;
                CLKCON = saved_clk;
                return i;            /* end of burst (partial capture) */
            }
        }
        TCON &= ~0x02;
        buf[i] = RFD;
    }

    radio_dbg_marc  = MARCSTATE & 0x1F;
    radio_dbg_pktst = PKTSTATUS;
    radio_rssi = RSSI; radio_lqi = LQI; radio_freqest = FREQEST;
    RFST = STROBE_IDLE;
    CLKCON = saved_clk;
    return n;
}

/*
 * Restart the radio after a PM2 sleep. PM2 stops the oscillators but RETAINS
 * all SFRs and RAM, so the full radio config (including the AFC'd FSCTRL0)
 * survives - we only need to spin the crystal back up and drop to IDLE.
 */
void radio_wake(void) {
    radio_ready = select_xosc();
    if (!radio_ready) return;
    RFST = STROBE_IDLE;
    wait_idle();
}

void radio_idle(void) {
    RFST = STROBE_IDLE;
}

void radio_sleep(void) {
    RFST = STROBE_IDLE;
    wait_idle();
}
