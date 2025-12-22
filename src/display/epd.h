#ifndef _EPD_H_
#define _EPD_H_

#include <stdint.h>

#define B_PWR 0   // P0_0
#define B_CS 1    // P0_1
#define B_DC 2    // P1_2 - low command, high data
#define B_BUSY 3  // P1_3 - low busy
#define B_RESET 0 // P2_0 - low reset

#define EPD_PWR P0_0
#define EPD_CS P0_1
#define EPD_DC P1_2
#define EPD_BUSY P1_3
#define EPD_RESET P2_0

#define EPD_PWR_ON EPD_PWR = 0
#define EPD_PWR_OFF EPD_PWR = 1
#define EPD_RESET_ON EPD_RESET = 0
#define EPD_RESET_OFF EPD_RESET = 1
#define EPD_SELECT EPD_CS = 0
#define EPD_DESELECT EPD_CS = 1
#define EPD_CMDMODE EPD_DC = 0
#define EPD_DATAMODE EPD_DC = 1

#define EPD_SIZE 266


#if (EPD_SIZE == 213)
#define HRES 104
#define VRES 212
extern const uint8_t register_data[];
#elif (EPD_SIZE == 266)
#define HRES 152
#define VRES 296
extern const uint8_t register_data[];
#elif (EPD_SIZE == 417)
#define HRES 400
#define VRES 300
extern const uint8_t register_data[];
#endif

#define BUFFER_SIZE (HRES / 8 * VRES)

void epd_reset(uint32_t ms1, uint32_t ms2, uint32_t ms3, uint32_t ms4, uint32_t ms5);
void epd_softReset(void);
void epd_waitBusy(void);
void epd_init(void);
void epd_spi_disable(void);
void epd_clearDisplay(void);
void epd_globalUpdate(const uint8_t * data1s, const uint8_t * data2s);
void epd_sendIndexData( uint8_t index, const uint8_t *data, uint32_t len );
void epd_sendColor( uint8_t index, const uint8_t data, uint32_t len );
void epd_flushDisplay(void);
void epd_DCDC_powerOn(void);
void epd_DCDC_powerOff(void);
void epd_powerOn(void);
void epd_powerOff(void);
void epd_sendCommand(uint8_t cmd);
void epd_sendData(uint8_t data);

void epd_stream_start(uint8_t index);
void epd_stream_data(const uint8_t *data, uint32_t len);
void epd_stream_end(void);

void epd_setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void epd_drawBlackRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void epd_setBWMode(uint8_t bw_only);

#endif