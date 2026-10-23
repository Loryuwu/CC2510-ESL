#ifndef RADIO_H
#define RADIO_H

#include <stdint.h>

#define RADIO_MAX_PAYLOAD 29

#define RADIO_ADDR_BROADCAST 0xFF

void radio_init(uint8_t addr);
void radio_set_addr(uint8_t addr);   /* change the protocol address at runtime */
/* Retune to nRF24 channel `ch` (carrier = 2400+ch MHz) at runtime, e.g. to
 * dodge WiFi. ch 76 = 2476 MHz = the compiled home channel. Auto-cal on the
 * next RX/TX strobe picks up the new frequency. */
void radio_set_channel(uint8_t ch);
uint8_t radio_tx(const uint8_t *buf, uint8_t len);
uint8_t radio_rx(uint8_t *buf, uint8_t *len, uint16_t timeout_ms);

/*
 * Diagnostic: capture raw OTA bytes exactly as the demodulator delivers
 * them after sync-word (A7 5C) detection - NO address filter, NO length
 * parse, NO XOR check, NO bit shift. Returns the number of bytes actually
 * captured (0 = never achieved sync lock within timeout). Use with a
 * loopback to see the ground truth of what the CC2510 receiver captures.
 */
uint8_t radio_rx_raw(uint8_t *buf, uint8_t maxlen, uint16_t timeout_ms);
uint8_t radio_get_marcstate(void);
void radio_wake(void);   /* restart the crystal + radio after a PM2 sleep */

/* Count of synced-but-rejected (corrupt / not-for-us) frames. The caller can
 * watch it change to flash a "bad packet" indicator vs. plain idle. */
extern __xdata uint8_t radio_rx_bad;

/* Link metrics of the last received burst (gateway->CC path). */
extern __xdata uint8_t radio_rssi;     /* CC2510 RSSI register (signed/2 - 71 = dBm) */
extern __xdata uint8_t radio_lqi;      /* lower 7 bits: lower = better demod */
extern __xdata uint8_t radio_freqest;  /* signed frequency offset estimate */
void radio_idle(void);
void radio_sleep(void);

#endif
