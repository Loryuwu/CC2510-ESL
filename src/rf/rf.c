#include "rf.h"
#include "../hal/hal.h"
#include "../hal/time.h"

// Strobe commands
#define SFSTXON 0x00
#define SCAL    0x01
#define SRX     0x02
#define STX     0x03
#define SIDLE   0x04
#define SFRX    0x06
#define SFTX    0x07
#define SNOP    0x0D

void rf_init(void) {
    // RF Settings for 250kBaud, MSK, 2433MHz
    // Generated/Verified for SmartRF Studio compatibility
    
    // Frequency Control
    FSCTRL1   = 0x0A;
    FSCTRL0   = 0x00;
    FREQ2     = 0x5D;
    FREQ1     = 0x93;
    FREQ0     = 0xB1;
    
    // Modem Configuration
    MDMCFG4   = 0x2D;
    MDMCFG3   = 0x3B;
    MDMCFG2   = 0x73; // MSK, 30/32 sync bits detected
    MDMCFG1   = 0x22; // FEC disabled, Preamble 4 bytes
    MDMCFG0   = 0xF8;
    
    // Channel and Deviation
    CHANNR    = 0x00;
    DEVIATN   = 0x00; // Not used for MSK
    
    // Front End
    FREND1    = 0xB6;
    FREND0    = 0x10;
    
    // State Machine
    MCSM0     = 0x18; // Calibrate on IDLE->TX/RX, PO_TIMEOUT 64
    MCSM1     = 0x00; // CCA always (00), RX->IDLE, TX->IDLE
    MCSM2     = 0x07; // RX_TIME_RSSI, RX_TIME_QUAL, RX_TIME
    
    // Frequency Offset Compensation
    FOCCFG    = 0x1D;
    BSCFG     = 0x1C;
    
    // AGC
    AGCCTRL2  = 0xC7;
    AGCCTRL1  = 0x00;
    AGCCTRL0  = 0xB2;
    
    // Calibration
    FSCAL3    = 0xEA;
    FSCAL2    = 0x0A;
    FSCAL1    = 0x00;
    FSCAL0    = 0x11;
    
    // Output Power
    PA_TABLE0 = 0xFE; // 0dBm
    
    // Packet Control
    PKTCTRL1  = 0x04; // Append Status (RSSI/LQI)
    PKTCTRL0  = 0x05; // Whitening OFF, CRC on, Variable Length
    PKTLEN    = 0xFF; // Max packet length
    
    // Sync Word (Default is D391, but good to be explicit)
    SYNC1     = 0xD3;
    SYNC0     = 0x91;
    
    // Address (Default 0x00, filtering disabled by default in PKTCTRL1)
    ADDR      = 0x00;

    RFST = SIDLE;
}

uint8_t rf_send_packet(uint8_t* data, uint8_t length) {
    // Force IDLE first
    RFST = SIDLE;
    while ((MARCSTATE & 0x1F) != 0x01); // Wait for IDLE
    
    // Clear IRQ_DONE
    RFIF &= ~0x10;
    
    // Write data to FIFO
    RFD = length;
    for (uint8_t i = 0; i < length; i++) {
        RFD = data[i];
    }
    
    // Start Transmission
    RFST = STX;
    
    // Wait for transmission to complete (MARCSTATE -> IDLE)
    // MCSM1 is configured to go to IDLE after TX.
    // We wait for MARCSTATE to become IDLE (0x01).
    uint16_t timeout = 0;
    while ((MARCSTATE & 0x1F) != 0x01) {
        timeout++;
        if (timeout > 30000) { // Increased timeout
            // Timeout: Force IDLE and return error
            RFST = SIDLE;
            return 0;
        }
        delay_ms(1); 
    }
    
    // Clear IRQ_DONE just in case
    RFIF &= ~0x10;
    
    return 1;
}

uint8_t rf_receive_packet(uint8_t* buffer, uint8_t max_length) {
    uint8_t status;
    uint8_t len;

    // Check if we are in RX, if not, enter RX
    if ((MARCSTATE & 0x1F) != 0x0D) {
        RFST = SRX;
        return 0;
    }

    // Check for IRQ_DONE (Bit 4 of RFIF)
    if (RFIF & 0x10) {
        // Clear interrupt flag
        RFIF &= ~0x10;
        
        // Read length byte
        len = RFD;
        
        if (len > max_length) {
            // Packet too long, flush RX FIFO
            RFST = SIDLE;
            RFST = SFRX; 
            
            // Go back to RX
            RFST = SRX;
            return 0;
        }
        
        for (uint8_t i = 0; i < len; i++) {
            buffer[i] = RFD;
        }
        
        // Read status bytes (RSSI, LQI) appended if configured (default is yes)
        // But if we read 'len' bytes, and 'len' was the payload length...
        // The length byte does not include the length byte itself.
        // It includes payload.
        // The status bytes are appended AFTER payload.
        // So we should read 2 more bytes.
        volatile uint8_t rssi = RFD;
        volatile uint8_t lqi = RFD;
        
        return len;
    }
    
    return 0;
}
