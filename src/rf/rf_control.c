/*
    RF Control Module for CC2510
    Standalone module
*/

#include "rf_control.h"
#include "../hal/hal.h"
#include "../hal/time.h"

// ============================================================================
// Definitions & Macros (Copied from hal_defines.h, hal_dma.h, hal_cc25xx.h)
// ============================================================================

#define HI(a)     (uint8_t) ((uint16_t)(a) >> 8 )
#define LO(a)     (uint8_t)  (uint16_t)(a)
#define SET_WORD(H, L, val) { (H) = HI(val); (L) = LO(val); }

// DMA Configuration
typedef struct {
    uint8_t SRCADDRH;
    uint8_t SRCADDRL;
    uint8_t DESTADDRH;
    uint8_t DESTADDRL;
    uint8_t LENH      : 5;
    uint8_t VLEN      : 3;
    uint8_t LENL      : 8;
    uint8_t TRIG      : 5;
    uint8_t TMODE     : 2;
    uint8_t WORDSIZE  : 1;
    uint8_t PRIORITY  : 2;
    uint8_t M8        : 1;
    uint8_t IRQMASK   : 1;
    uint8_t DESTINC   : 2;
    uint8_t SRCINC    : 2;
} HAL_DMA_DESC;

// DMA Constants
#define DMA_VLEN_FIRST_BYTE_P_1  0x01
#define DMA_VLEN_FIRST_BYTE_P_3  0x04
#define DMA_WORDSIZE_BYTE        0x00
#define DMA_TMODE_SINGLE         0x00
#define DMA_TRIG_RADIO           19
#define DMA_SRCINC_0             0x00
#define DMA_SRCINC_1             0x01
#define DMA_DESTINC_0            0x00
#define DMA_DESTINC_1            0x01
#define DMA_IRQMASK_DISABLE      0x00
#define DMA_M8_USE_8_BITS        0x00
#define DMA_PRI_HIGH             0x02
#define DMA_ARM_ABORT            0x80
#define DMA_ARM_CH0              (1<<0)

// RF Constants
#define RFST_SIDLE               0x04
#define RFST_STX                 0x03
#define RFST_SRX                 0x02
#define CC25XX_MODE_RX           0
#define CC25XX_MODE_TX           1

// ============================================================================
// Internal State
// ============================================================================

static __xdata HAL_DMA_DESC rf_dma_config[1]; // Only using 1 channel (CH0)
static __xdata uint8_t rf_buffer[64]; // Internal buffer
static uint8_t rf_mode;

// ============================================================================
// Internal Helper Functions
// ============================================================================

static void rf_setup_dma(uint8_t mode) {
    rf_dma_config[0].PRIORITY       = DMA_PRI_HIGH;
    rf_dma_config[0].M8             = DMA_M8_USE_8_BITS;
    rf_dma_config[0].IRQMASK        = DMA_IRQMASK_DISABLE;
    rf_dma_config[0].TRIG           = DMA_TRIG_RADIO;
    rf_dma_config[0].TMODE          = DMA_TMODE_SINGLE;
    rf_dma_config[0].WORDSIZE       = DMA_WORDSIZE_BYTE;

    rf_mode = mode;

    if (rf_mode == CC25XX_MODE_TX) {
        // TX: Buffer -> RFD
        SET_WORD(rf_dma_config[0].SRCADDRH, rf_dma_config[0].SRCADDRL, rf_buffer);
        SET_WORD(rf_dma_config[0].DESTADDRH, rf_dma_config[0].DESTADDRL, &X_RFD);
        rf_dma_config[0].VLEN           = DMA_VLEN_FIRST_BYTE_P_1;
        // Max length: assuming first byte is length
        SET_WORD(rf_dma_config[0].LENH, rf_dma_config[0].LENL, 64); 
        rf_dma_config[0].SRCINC         = DMA_SRCINC_1;
        rf_dma_config[0].DESTINC        = DMA_DESTINC_0;
    } else {
        // RX: RFD -> Buffer
        SET_WORD(rf_dma_config[0].SRCADDRH, rf_dma_config[0].SRCADDRL, &X_RFD);
        SET_WORD(rf_dma_config[0].DESTADDRH, rf_dma_config[0].DESTADDRL, rf_buffer);
        rf_dma_config[0].VLEN           = DMA_VLEN_FIRST_BYTE_P_3; // +2 status bytes
        SET_WORD(rf_dma_config[0].LENH, rf_dma_config[0].LENL, 64);
        rf_dma_config[0].SRCINC         = DMA_SRCINC_0;
        rf_dma_config[0].DESTINC        = DMA_DESTINC_1;
    }

    // Load DMA config to Channel 0
    SET_WORD(DMA0CFGH, DMA0CFGL, &rf_dma_config[0]);
}

static void rf_enter_rx(void) {
    // Abort any ongoing DMA
    DMAARM = DMA_ARM_ABORT | DMA_ARM_CH0;
    
    rf_setup_dma(CC25XX_MODE_RX);
    
    // Arm DMA
    DMAARM = DMA_ARM_CH0;
    
    // Strobe RX
    RFST = RFST_SRX;
}

// ============================================================================
// Public API Implementation
// ============================================================================

void rf_control_init(void) {
    // Basic initialization
    // Assuming global interrupts and system clock are set up by the main application
    
    // RF Settings for 250kBaud, MSK, 2433MHz
    // Copied from rf.c for compatibility
    
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
    
    // Sync Word
    SYNC1     = 0xD3;
    SYNC0     = 0x91;
    
    // Address
    ADDR      = 0x00;

    RFST = RFST_SIDLE;
    
    // Set priority for RF (IP1.0, IP0.0)
    IP1 |= (1<<0);
    IP0 |= (1<<0);

    // Default to RX
    rf_enter_rx();
}

void rf_control_send_packet(uint8_t *data, uint8_t len) {
    if (len > 60) len = 60; // Safety limit

    // Prepare buffer: [Length, Data...]
    rf_buffer[0] = len;
    for (uint8_t i = 0; i < len; i++) {
        rf_buffer[i+1] = data[i];
    }

    // Abort current DMA
    DMAARM = DMA_ARM_ABORT | DMA_ARM_CH0;

    // Setup DMA for TX
    rf_setup_dma(CC25XX_MODE_TX);

    // Strobe STX
    RFST = RFST_STX;

    // Arm DMA
    DMAARM = DMA_ARM_CH0;

    // Trigger transmission (S1CON |= 0x03 forces interrupt/trigger)
    // The original code uses S1CON |= 0x03.
    S1CON |= 0x03;

    // Wait for completion
    // We can check if DMA is done or RF interrupt flag
    // Polling RFIF bit 4 (IRQ_DONE)
    while (!(RFIF & (1<<4)));
    
    // Clear IRQ_DONE
    RFIF &= ~(1<<4);
    
    // Return to RX
    rf_enter_rx();
}

uint8_t rf_control_receive_packet(uint8_t *buffer, uint8_t max_len) {
    // Check if packet received
    // In RX mode, DMA should have transferred data to rf_buffer
    // We can check if DMA channel 0 is no longer armed (if it finishes?)
    // Or check RFIF IRQ_DONE
    
    if (RFIF & (1<<4)) {
        // Packet received (or TX done, but we are in RX mode)
        if (rf_mode == CC25XX_MODE_RX) {
            // Clear flag
            RFIF &= ~(1<<4);
            
            // Get length from first byte
            uint8_t len = rf_buffer[0];
            
            // Check CRC (last byte of status) - Optional but good
            // Status bytes are at rf_buffer[len+1] and rf_buffer[len+2]
            // uint8_t lqi = rf_buffer[len+1];
            // uint8_t crc = rf_buffer[len+2];
            // if (crc & 0x80) { // CRC OK }
            
            if (len > max_len) len = max_len;
            
            for (uint8_t i = 0; i < len; i++) {
                buffer[i] = rf_buffer[i+1];
            }
            
            // Re-arm RX
            rf_enter_rx();
            
            return len;
        }
    }
    
    return 0;
}

void rf_control_set_channel(uint8_t channel) {
    // CHANNR register is 0xDF36 (SFR) or similar? 
    // Wait, CHANNR is an SFR in CC2510.
    // In cc2510fx.h it should be defined as CHANNR.
    CHANNR = channel;
}
