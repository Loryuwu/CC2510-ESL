#include "nfc.h"
#include "i2c.h"
#include "../hal/hal.h"
#include "../hal/time.h"

#define NFC_ADDR_WRITE 0xAA
#define NFC_ADDR_READ  0xAB

void nfc_init(void) {
    // 1. Power Up NFC (P1_0)
    // Make sure P1_0 is Output
    P1DIR |= (1<<0);
    P1_0 = 1;
    
    // Wait for boot (datasheet says ~200us?)
    delay_ms(5); // Safe margin
    
    // 2. Init I2C
    i2c_init();
}

bool nfc_read_page(uint8_t page, uint8_t *buffer) {
    i2c_start();
    
    // Address + Write (to send Register/Page address)
    if (!i2c_write_byte(NFC_ADDR_WRITE)) {
        i2c_stop();
        return false;
    }
    
    // Send Page Address
    if (!i2c_write_byte(page)) {
        i2c_stop();
        return false;
    }
    
    // Restart for Read
    i2c_start();
    
    // Address + Read
    if (!i2c_write_byte(NFC_ADDR_READ)) {
        i2c_stop();
        return false;
    }
    
    // Read 16 Bytes (NT3H2111 Page Size)
    for (uint8_t i = 0; i < 16; i++) {
        // Send ACK for all except last byte
        buffer[i] = i2c_read_byte(i < 15);
    }
    
    i2c_stop();
    return true;
}

bool nfc_write_page(uint8_t page, uint8_t *data) {
    i2c_start();
    
    if (!i2c_write_byte(NFC_ADDR_WRITE)) {
        i2c_stop();
        return false;
    }
    
    // Page Address
    if (!i2c_write_byte(page)) {
        i2c_stop();
        return false;
    }
    
    // Write 16 Bytes
    for (uint8_t i = 0; i < 16; i++) {
        if (!i2c_write_byte(data[i])) {
            i2c_stop();
            return false;
        }
    }
    
    i2c_stop();
    
    // Wait for Write Cycle (approx 5-10ms for EEPROM)
    delay_ms(10);
    
    return true;
}

bool nfc_read_session_reg(uint8_t *reg_val) {
    (void)reg_val;
    return false;
}

// Erase All (Factory Reset - User Memory Only)
// Erases User Memory (Block 1..0x39 / Page 4..0xE2)
// DOES NOT Reset Configuration to avoid losing I2C Addr
// Erase All (Factory Reset - User Memory Only)
// Erases User Memory (I2C Page 1..0x39) -> (NFC Block 4..0xE7)
// DOES NOT Reset Configuration to avoid losing I2C Addr
void nfc_erase_all(void) {
    static __xdata uint8_t zero_buf[16] = {0};
    
    // 0. Reset Page 0 (UID + Static Locks + CC)
    // Bytes 0-9: UID (Read Only - Writes ignored)
    // Bytes 10-11: Static Lock (0x00 0x00 = Open)
    // Bytes 12-15: CC (E1 10 6D 0F = NDEF Formatted)
    static __xdata uint8_t page0_buf[16] = {
        0x04, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, // 0-9 (Ignored)
        0xFF, 0xFF, // 10-11 (Locks Open)
        0xE1, 0x10, 0x6D, 0x0F // 12-15 (CC)
    };
//    nfc_write_page(0, page0_buf); // provoca error critico en el chip, queda inutilizable

    // 1. Erase User Memory
    // I2C Page 1 starts User Memory.
    // I2C Page 0x37 ends User Memory (Block 0xDF)
    for (uint8_t i = 1; i <= 0x37; i++) {
        nfc_write_page(i, zero_buf);
    }
    
    // Config Page (I2C 0x38 -> NFC Blocks E0, E1, E2, E3)
    // E0-E2 = 0x00
    // E3: [NC_REG] [REG_LOCK] [ACCESS] [AUTH0]
    // We set:
    // ACCESS (Byte 2) = 0x00 (Write access granted, Prot bit = 0)
    // AUTH0  (Byte 3) = 0xFF (Start protection at Page 255 -> Disabled)
    static __xdata uint8_t config_buf[16] = {
        0,0,0,0, 0,0,0,0, 0,0,0,0,  // E0, E1, E2
        0x00, 0x00, 0x00, 0xFF      // E3 (ACCESS=00, AUTH0=FF)
    };
    nfc_write_page(0x38, config_buf);
}

uint8_t nfc_scan(void) {
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_start();
        // Try to write to address. 
        // Shift left by 1 because i2c_write_byte expects 8-bit addr (7-bit + R/W bit)
        // We send Write bit (0)
        bool ack = i2c_write_byte(addr << 1); 
        i2c_stop();
        
        if (ack) {
            return addr; // Return the 7-bit address found
        }
    }
    return 0; // None found
}
