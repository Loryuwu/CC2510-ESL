#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>
#include "../hal/hal.h"

// SPI Flash Commands
#define CMD_WRITE_ENABLE      0x06
#define CMD_WRITE_DISABLE     0x04
#define CMD_READ_STATUS_REG   0x05
#define CMD_WRITE_STATUS_REG  0x01
#define CMD_READ_DATA         0x03
#define CMD_FAST_READ         0x0B
#define CMD_PAGE_PROGRAM      0x02
#define CMD_SECTOR_ERASE      0x20
#define CMD_BLOCK_ERASE       0xD8
#define CMD_CHIP_ERASE        0xC7
#define CMD_POWER_DOWN        0xB9
#define CMD_RELEASE_POWER_DOWN 0xAB
#define CMD_MANUFACTURER_ID   0x90
#define CMD_JEDEC_ID          0x9F

// Function Prototypes
void spi_flash_init(void);
uint16_t spi_flash_read_id(void);
void spi_flash_read(uint32_t addr, uint8_t *buf, uint16_t len);
void spi_flash_write(uint32_t addr, const uint8_t *buf, uint16_t len);
void spi_flash_sector_erase(uint32_t addr);
void spi_flash_chip_erase(void);
void spi_flash_power_down(void);
void spi_flash_release_power_down(void);
void spi_flash_wait_busy(void);

#endif // FLASH_H
