#ifndef NFC_H
#define NFC_H

#include <stdint.h>

/*
 * Driver for the NXP NT3H2111 (NTAG I2C plus 2k) on the ESL board.
 * Bit-banged I2C on P0.4 (SDA) / P0.6 (SCL), chip powered via P1.0.
 *
 * Memory is addressed in 16-byte I2C blocks:
 *   block 0       = UID / lock / Capability Container (read-only identity)
 *   block 1..     = user memory (safe to read/write)
 * We reserve I2C block 1 for PriceTag config (magic + assigned address).
 */

#define NFC_CFG_BLOCK  8          /* user-memory block for PriceTag config */
#define NFC_MAGIC0     0x50       /* 'P' */
#define NFC_MAGIC1     0x54       /* 'T' */

/* Config block layout (16 bytes at NFC_CFG_BLOCK):
 *   [0-1] magic PT  [2-3] poll_seconds (big-endian)  [4] channel
 *   [5] flags (bit0=force_refresh)  [6-15] reserved */
#define NFC_CFG_FLAG_REFRESH 0x01

/* Status block (written by firmware, read by phone) */
#define NFC_STATUS_BLOCK 9

void    nfc_init(void);

uint8_t nfc_read_block(uint8_t block, uint8_t *buf16);
uint8_t nfc_write_block(uint8_t block, const uint8_t *buf16);

uint8_t nfc_read_uid(uint8_t *uid7);
uint8_t nfc_read_label(uint8_t *lbl4);

/* Write NDEF URI record so phones open the HA dashboard on tap.
 * label4 = 4-byte tag label (hex in URL). Uses blocks 1-3. */
void nfc_write_ndef_url(const uint8_t *label4);

/* Write status info to NFC_STATUS_BLOCK (readable by phone NFC tools). */
void nfc_write_status(uint8_t rssi, uint8_t lqi, uint16_t polls, uint8_t misses);

/* Read config from NFC_CFG_BLOCK. Returns 1 if magic valid.
 * poll_s/channel/flags filled only when magic matches. */
uint8_t nfc_read_config(uint16_t *poll_s, uint8_t *channel, uint8_t *flags);

#endif
