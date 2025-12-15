#ifndef NFC_H
#define NFC_H

#include <stdint.h>
#include <stdbool.h>

// NT3H2111 Constants
#define NFC_I2C_ADDR_DEFAULT 0xAA // 8-bit Address (Shifted? No, usually 0x55 << 1 = 0xAA)
// If 7-bit is 0x55. 
// Code uses 8-bit address conventions usually (Write=Addr, Read=Addr|1)

// Initialize NFC Module (Powers up and Inits I2C)
void nfc_init(void);

// Read 16 bytes from a page
// page: Page Address (0-E2 for NTAG I2C 2k)
// buffer: Pointer to 16-byte buffer
// Returns true on success
bool nfc_read_page(uint8_t page, uint8_t *buffer);

// Write 16 bytes to a page
// page: Page Address
// data: Pointer to 16-byte data
// Returns true on success
bool nfc_write_page(uint8_t page, uint8_t *data);

// Read Session Register (Get FD status etc)
// mask_reg: Pointer to store NC_REG (if available in this chip)
bool nfc_read_session_reg(uint8_t *reg_val);

// Erase all user pages (Page 4 to End)
// Fills with 0x00
// Erase all user pages (Page 4 to End)
// Fills with 0x00
void nfc_erase_all(void);

// Scan I2C bus for devices
// Returns the first 7-bit address found, or 0 if none.
uint8_t nfc_scan(void);

#endif // NFC_H
