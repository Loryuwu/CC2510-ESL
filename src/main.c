#include "display/epd.h"

#include "hal/clock.h"
#include "hal/hal.h"
#include "hal/isr.h"
#include "hal/led.h"
#include "hal/time.h"
#include "hal/uart.h"

#include "image_data/image.h"

#include "cobs/cobs.h"

#include "rf/rf.h"


void main(void) {
  init_clock();
  time_init();
  //uart_init();
  
  HAL_ENABLE_INTERRUPTS();
  LED_INIT;

  for (uint8_t i = 0; i < 10; i++) {
    LED_TOGGLE;
    delay_ms(50);
  }
  delay_ms(500);


/****************************
 * 
 *  Inicialización de la pantalla EPD 
 * 
 *****************************/

  // epd_init();
  // LED_B_ON;
  // epd_clearDisplay();
  // sendIndexData( 0x10, image2, BUFFER_SIZE); // First frame
  // sendColor( 0x13, 0x00, BUFFER_SIZE); // Second frame
  // flushDisplay();
  // delay_ms(1000);
  // LED_B_OFF;
  // sleep_for_minutes(0);
  // sleep_test();
  // LED_G_ON;


  while (1)
  {
  LED_B_ON;
  // PWR_ON;
  // sendIndexData( 0x10, image1, BUFFER_SIZE); // First frame
  // sendColor( 0x13, 0x00, BUFFER_SIZE); // Second frame
  // flushDisplay();
  delay_ms(1000);
  LED_B_OFF;
  sleep_for_minutes(1);
  }
  

/****************************
 * 
 *  Inicialización de RF
 * 
 *****************************/
  delay_ms(500);
  rf_init();
  static __xdata uint8_t data[] = "Hola mundo desde RF!";
  uint8_t data_length = sizeof(data) - 1; // Restamos 1 para no enviar el caracter nulo
  // LED_R_OFF;
  // LED_G_OFF;
  // LED_B_OFF;
  // LED_OFF;
  for (uint8_t i = 0; i < 10; i++) {
    LED_TOGGLE;
    delay_ms(50);
  } //Parpadeo significa exito en la inicialización RF


  // while (1)
  // {
  //   if (rf_send_packet(data, data_length)) {
  //     // Éxito: Parpadeo verde rápido
  //     for(uint8_t i = 0; i < 3; i++) {
  //       LED_G_ON;
  //       delay_ms(100);
  //       LED_G_OFF;
  //       delay_ms(100);
  //     }
  //   } else {
  //     // Error: Parpadeo rojo lento
  //     for(uint8_t i = 0; i < 2; i++) {
  //       LED_R_ON;
  //       delay_ms(200);
  //       LED_R_OFF;
  //       delay_ms(200);
  //     }
  //   }
    
  //   // Mostrar estado actual del radio
  //   uint8_t state = MARCSTATE;
  //   for(uint8_t i = 0; i < state; i++) {
  //     LED_B_ON;
  //     delay_ms(50);
  //     LED_B_OFF;
  //     delay_ms(50);
  //   }
    
  //   delay_ms(500);  // Pausa entre intentos
  // }
  
  // CobsState __xdata cobsState;
  // cobs_init(&cobsState);

  // uint32_t __xdata next = 0;
  // while (1) {
  //   if (cobs_handle(&cobsState)) {
  //     cobs_send(cobsState.packet, cobsState.packet_size);
  //     LED_TOGGLE;
  //   }

  //   if (millis() >= next) {
  //     next += 1000;
  //     LED_TOGGLE;
  //   }
  // }
}