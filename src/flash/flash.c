#include "flash.h"
#include "../hal/hal.h"

// CS Pin Macros (P1_4)
#define FLASH_CS_LOW()    st(P1_4 = 0;)
#define FLASH_CS_HIGH()   st(P1_4 = 1;)

// Helper function to send/receive a byte
static uint8_t spi_transfer(uint8_t data) {
    U1DBUF = data;
    while (U1CSR & 0x01); // Wait for active bit to clear (transfer complete)
    return U1DBUF;
}

void spi_flash_init(void) {
    // 1. Configure USART1 for SPI Alt 2
    // PERCFG.U1CFG = 1 (Alt 2)
    PERCFG |= 0x02; 

    // 2. Configure SPI Mode
    // U1CSR: Mode = SPI Master (Bit 7 = 0), SPI Enable (Bit 6 = 1 later)
    U1CSR = 0x00; 

    // U1GCR: CPOL=0, CPHA=0, Order=MSB First (Bit 5=1)
    // Baud_E = 17 (similar to EPD)
    U1GCR = BV(5) | 17; 
    U1BAUD = 0x00; // Baud_M = 0

    // 3. Configure Pins
    // P1_4 (CS) -> Output
    // P1_5 (CLK), P1_6 (MOSI) -> Peripheral (Alt 2)
    // P1_7 (MISO) -> Peripheral (Alt 2)
    
    // Set P1_5, P1_6, P1_7 to Peripheral function
    P1SEL |= BV(5) | BV(6) | BV(7);
    
    // Set P1_4 to Output (CS)
    P1DIR |= BV(4);
    
    // Initialize CS High
    FLASH_CS_HIGH();

    // Enable SPI
    U1CSR |= BV(6);
}

void spi_flash_wait_busy(void) {
    uint8_t status;
    FLASH_CS_LOW();
    spi_transfer(CMD_READ_STATUS_REG);
    do {
        status = spi_transfer(0xFF);
    } while (status & 0x01); // WIP bit (Write In Progress)
    FLASH_CS_HIGH();
}

uint16_t spi_flash_read_id(void) {
    uint16_t id = 0;
    FLASH_CS_LOW();
    spi_transfer(CMD_MANUFACTURER_ID);
    spi_transfer(0x00);
    spi_transfer(0x00);
    spi_transfer(0x00); // Dummy bytes
    id = spi_transfer(0xFF) << 8;
    id |= spi_transfer(0xFF);
    FLASH_CS_HIGH();
    return id;
}

void spi_flash_read(uint32_t addr, uint8_t *buf, uint16_t len) {
    spi_flash_wait_busy();
    FLASH_CS_LOW();
    spi_transfer(CMD_READ_DATA);
    spi_transfer((addr >> 16) & 0xFF);
    spi_transfer((addr >> 8) & 0xFF);
    spi_transfer(addr & 0xFF);
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = spi_transfer(0xFF);
    }
    FLASH_CS_HIGH();
}

void spi_flash_write(uint32_t addr, const uint8_t *buf, uint16_t len) {
    // Write Enable
    spi_flash_wait_busy();
    FLASH_CS_LOW();
    spi_transfer(CMD_WRITE_ENABLE);
    FLASH_CS_HIGH();

    // Page Program
    spi_flash_wait_busy();
    FLASH_CS_LOW();
    spi_transfer(CMD_PAGE_PROGRAM);
    spi_transfer((addr >> 16) & 0xFF);
    spi_transfer((addr >> 8) & 0xFF);
    spi_transfer(addr & 0xFF);
    for (uint16_t i = 0; i < len; i++) {
        spi_transfer(buf[i]);
    }
    FLASH_CS_HIGH();
}

void spi_flash_sector_erase(uint32_t addr) {
    // Write Enable
    spi_flash_wait_busy();
    FLASH_CS_LOW();
    spi_transfer(CMD_WRITE_ENABLE);
    FLASH_CS_HIGH();

    // Sector Erase
    spi_flash_wait_busy();
    FLASH_CS_LOW();
    spi_transfer(CMD_SECTOR_ERASE);
    spi_transfer((addr >> 16) & 0xFF);
    spi_transfer((addr >> 8) & 0xFF);
    spi_transfer(addr & 0xFF);
    FLASH_CS_HIGH();
}

void spi_flash_chip_erase(void) {
    // Write Enable
    spi_flash_wait_busy();
    FLASH_CS_LOW();
    spi_transfer(CMD_WRITE_ENABLE);
    FLASH_CS_HIGH();

    // Chip Erase
    spi_flash_wait_busy();
    FLASH_CS_LOW();
    spi_transfer(CMD_CHIP_ERASE);
    FLASH_CS_HIGH();
}

void spi_flash_power_down(void) {
    spi_flash_wait_busy();
    FLASH_CS_LOW();
    spi_transfer(CMD_POWER_DOWN);
    FLASH_CS_HIGH();
}

void spi_flash_release_power_down(void) {
    FLASH_CS_LOW();
    spi_transfer(CMD_RELEASE_POWER_DOWN);
    FLASH_CS_HIGH();
}
