#include "epd.h"
#include "../hal/hal.h"
#include "../hal/time.h"
#include "../hal/led.h"

// Define register_data array based on EPD_SIZE
#if (EPD_SIZE == 213)
const uint8_t register_data[] = { 0x00, 0x0e, 0x19, 0x02, 0xcf, 0x8d };	// other sizes
#elif (EPD_SIZE == 266)
const uint8_t register_data[] = { 0x00, 0x0e, 0x19, 0x02, 0xcf, 0x8d };	// other sizes
#elif (EPD_SIZE == 417)
const uint8_t register_data[] = { 0x00, 0x0e, 0x19, 0x02, 0x0f, 0x89 };	//4,2
#endif

void inline epd_waitBusy() {
  do {
  delay_ms(1);
  } while (EPD_BUSY == 0);
  delay_ms(200);
}

void epd_reset(uint32_t ms1, uint32_t ms2, uint32_t ms3, uint32_t ms4, uint32_t ms5)
{
  delay_ms(ms1);
  EPD_RESET_ON;
  delay_ms(ms2);
  EPD_RESET_OFF;
  delay_ms(ms3);
  EPD_RESET_ON;
  delay_ms(ms4);
  EPD_RESET_OFF;
  delay_ms(ms5);
}

void epd_softReset()
{
  epd_sendIndexData( 0x00, &register_data[1], 1);
  epd_waitBusy();
}

void epd_init() {
  PERCFG &= ~(0x01);  // USART0 alternative 1 location
  U0CSR = 0x00;          // SPI mode/master/clear flags
  U0GCR = BV(5) | 17; // SCK-low idle, DATA-1st clock edge, MSB first + baud E
  U0BAUD = 0x00;         // baud M
  U0CSR |= BV(6);     // enable SPI

  P0SEL |= BV(3) | BV(5);                        // MISO/MOSI/CLK peripheral functions
  P0DIR |= BV(3) | BV(5) | BV(B_PWR) | BV(B_CS); // MOSI/CLK, PWR/CS output
  P1DIR |= BV(B_DC);
  P1DIR &= ~BV(B_BUSY);
  P2DIR |= BV(B_RESET);

  EPD_PWR_ON;
  
  delay_ms(5);
  epd_reset(1, 5, 10, 5, 1);
  epd_waitBusy();
  epd_softReset();

  epd_sendIndexData( 0xe5, &register_data[2], 1 );  //Input Temperature: 25C
	epd_sendIndexData( 0xe0, &register_data[3], 1 );  //Active Temperature
	epd_sendIndexData( 0x00, &register_data[4], 2 );  //PSR

}

void epd_clearDisplay() {
  epd_sendColor(0x10, 0x00, BUFFER_SIZE);
  epd_sendColor(0x13, 0x00, BUFFER_SIZE);
  epd_flushDisplay();
}

void epd_globalUpdate(const uint8_t * data1s, const uint8_t * data2s)
{
  // send first frame
  epd_sendIndexData(0x10, data1s, BUFFER_SIZE); // First frame

  // send second frame
  epd_sendIndexData(0x13, data2s, BUFFER_SIZE); // Second frame

  epd_flushDisplay();
}

void epd_sendIndexData( uint8_t index, const uint8_t *data, uint32_t len )
{	
  epd_sendCommand(index);
  EPD_DATAMODE;
  EPD_SELECT;
  for ( uint32_t i = 0; i < len; i++){
    U0DBUF = data[i];
    while (U0CSR & 0x01) {
    }
  }
  EPD_DESELECT;

}

void epd_sendColor( uint8_t index, const uint8_t data, uint32_t len )
{	
  epd_sendCommand(index);
  EPD_DATAMODE;
  EPD_SELECT;
  for ( uint32_t i = 0; i < len; i++){
    U0DBUF = data;
    while (U0CSR & 0x01) {
    }
  }
  EPD_DESELECT;
}

void epd_flushDisplay()
{
  epd_DCDC_powerOn();
  // sendIndexData( 0x12, &register_data[0], 1 );	//Display Refresh
  epd_sendIndexData( 0x11, &register_data[0], 1 );	//Display Refresh
  epd_waitBusy();
  epd_DCDC_powerOff();
}

void epd_DCDC_powerOn()
{
  epd_sendIndexData( 0x04, &register_data[0], 1 );  //Power on DC/DC
  epd_waitBusy();
}

void epd_DCDC_powerOff()
{
  epd_sendIndexData( 0x02, &register_data[0], 0 );  //Turn off DC/DC
  epd_waitBusy();
}

void epd_powerOn()
{
  // epd_DCDC_powerOn();
  EPD_SELECT;
  // delay_ms(150);
  EPD_PWR_ON;
}

void epd_powerOff()
{
  epd_DCDC_powerOff();
  EPD_CMDMODE;
  EPD_SELECT;
  delay_ms(150);
  EPD_RESET_OFF;
  EPD_PWR_OFF;
}

void epd_sendData(uint8_t data) {
  EPD_DATAMODE;
  EPD_SELECT;
  U0DBUF = data;
  while (U0CSR & 0x01) {
  }
  EPD_DESELECT;
}

void epd_sendCommand(uint8_t cmd) {
  EPD_CMDMODE;
  EPD_SELECT;
  U0DBUF = cmd;
  while (U0CSR & 0x01) {
  }
  EPD_DESELECT;
}
