#include "display/epd.h"

#include "hal/clock.h"
#include "hal/hal.h"
#include "hal/isr.h"
#include "hal/led.h"
#include "hal/time.h"
#include "hal/uart.h"

#include "image_data/image.h"

#include "rf/rf.h"

#include "cobs/cobs.h"



void main(void) {
  init_clock(); //Cambiar de oscillador interno a externo
  time_init(); //Inicializa el timer para funciones de retardo y millis
  
  HAL_ENABLE_INTERRUPTS(); //Habilita interrupciones globales
  LED_INIT; //Inicializa pines de los LEDs

  for (uint8_t i = 0; i < 10; i++) {
    LED_TOGGLE;
    delay_ms(50);
  }  //Parpadeo significa exito en la inicialización del sistema

  delay_ms(500);

#if 0 //test display
  LED_B_ON;
  epd_init();
  epd_sendIndexData(0x10, image4 , BUFFER_SIZE);
  epd_sendColor(0x13, 0x00, BUFFER_SIZE);
  epd_flushDisplay();
  LED_B_OFF;
  epd_powerOff();
  while (1);
#endif

////////////////////////////////
//  Deep Sleep Test
 #if 0 // Cambiar a 1 para activar el test de deep sleep
////////////////////////////////

  LED_B_ON;
  delay_ms(1000);
  LED_B_OFF;
  delay_ms(5000);
  LED_B_ON;
  delay_ms(100);
  LED_B_OFF;

  // sleep_for_minutes(1);
  sleep_ms(500);
  // deep_sleep_seconds(2);
  LED_G_ON;

  for (uint8_t i = 0; i < 3; i++)
  {
    LED_B_ON;
    delay_ms(100);
    LED_B_OFF;
    delay_ms(100);
  }
  LED_B_OFF;
  LED_G_OFF;

#endif //Sleep
////////////////////////////////
//  Inicialización de la pantalla EPD 
#if 0 // Cambiar a 1 para activar el test de pantalla EPD
////////////////////////////////

  // Variable para alternar entre imágenes. 
  // Se declara static para mantener su valor entre reinicios si no fuera deep sleep, 
  // pero en deep sleep la RAM se mantiene (PM2).
  static uint8_t current_image = 0;

  while (1) {
    // 1. Inicializar pantalla
    epd_init(); 
    LED_B_ON; // Indicador visual breve

    // 2. Actualizar buffer según la imagen actual
    switch (current_image) {
        case 0:
            epd_sendIndexData(0x10, image1, BUFFER_SIZE);
            epd_sendColor(0x13, 0x00, BUFFER_SIZE); // Asumiendo que image1 es B/N y el canal rojo va vacío
            break;
        case 1:
            epd_sendIndexData(0x10, image2, BUFFER_SIZE);
            epd_sendColor(0x13, 0x00, BUFFER_SIZE);
            break;
        case 2:
            epd_sendIndexData(0x10, image3, BUFFER_SIZE);
            epd_sendColor(0x13, 0x00, BUFFER_SIZE);
            break;
    }

    // 3. Refrescar pantalla
    epd_flushDisplay();
    LED_B_OFF;
    // 4. Apagar pantalla (Deep Sleep del EPD)
    epd_powerOff();

    // 5. Alternar imagen para la próxima vez
    current_image += 1;
    if (current_image >= 3) {
      current_image = 0;
    }

    // 6. Dormir el MCU por x horas (en alguna parte hay un error, pero 1 segundo equivale a medio minuto realmente)
    // deep_sleep_seconds(120*6); //120 equivale a 1 hora, multiplicamos por x cantidad de horas (en este caso 6)
    delay_ms(5000);
  }


#endif //EPD
////////////////////////////////
//  Inicialización de la Comunicacion al flash externo
#if 0 // Cambiar a 1 para activar el test de memoria Flash
////////////////////////////////
  rf_init();
  spi_flash_init();

  uint16_t flash_id = spi_flash_read_id();

  while (1)
  {
    if (flash_id == 0xEF) {
      LED_ON;
      delay_ms(5000);
      LED_OFF;
      delay_ms(5000);
    }else {
      LED_ON;
      delay_ms(500);
      LED_OFF;
      delay_ms(500);
    }
  }

  // while (1)
  // {
  //   LED_B_ON;
  //   rf_send_packet(&flash_id, sizeof(flash_id));
  //   LED_B_OFF;
  //   delay_ms(5000);
  // }

#endif //Flash
////////////////////////////////
//  Inicialización de RF TX
#if 0 // Cambiar a 1 para activar el test de RF
////////////////////////////////
  delay_ms(500);
  LED_B_ON;
  rf_init();
  
  static __xdata uint8_t data[] = "\nHola mundo desde RF!\n¿Como esta el mundo?";
  uint8_t data_length = sizeof(data) - 1; // Restamos 1 para no enviar el caracter nulo
  delay_ms(50);
  LED_R_OFF;
  LED_G_OFF;
  LED_B_OFF;
  LED_OFF;
  
  for (uint8_t i = 0; i < 10; i++) {
    LED_TOGGLE;
    delay_ms(50);
  } //Parpadeo significa exito en la inicialización RF

  while (1)
  {
    LED_B_ON;
    rf_send_packet(data, data_length);
    LED_B_OFF;
    delay_ms(500);
    
    for(uint8_t i = 0; i < 5; i++) {
      LED_G_ON;
      delay_ms(50);
      LED_G_OFF;
      delay_ms(50);
    }
    
    delay_ms(1000);  // Pausa entre intentos
  }
#endif //RF TX
////////////////////////////////
//  Inicialización de RF RX
#if 0 // Cambiar a 1 para activar el test de RF
////////////////////////////////
  rf_init();
  epd_init(); 
  
  // Buffer para datos recibidos
  static __xdata uint8_t rx_buffer[64];
  
  // Parpadeo para indicar inicio
  for (uint8_t i = 0; i < 5; i++) {
    LED_B_ON;
    delay_ms(50);
    LED_B_OFF;
    delay_ms(50);
  }

  while (1) {
    // Intentar recibir un paquete
    uint8_t len = rf_receive_packet(rx_buffer, 63);
    
    if (len > 0) {
      // Verificar si el paquete comienza con "LED"
      if (len >= 3 && rx_buffer[0] == 'L' && rx_buffer[1] == 'E' && rx_buffer[2] == 'D') {
        epd_powerOn();
        if (rx_buffer[4] == 'R') {
          LED_R_ON;
          epd_sendIndexData(0x10, image1, BUFFER_SIZE);
          epd_sendColor(0x13, 0x00, BUFFER_SIZE);
          LED_R_OFF;
        } else if (rx_buffer[4] == 'G') {
          LED_G_ON;
          epd_sendIndexData(0x10, image2, BUFFER_SIZE);
          epd_sendColor(0x13, 0x00, BUFFER_SIZE);
          LED_G_OFF;
        } else if (rx_buffer[4] == 'B')  {
          LED_B_ON;
          epd_sendIndexData(0x10, image3, BUFFER_SIZE);
          epd_sendColor(0x13, 0x00, BUFFER_SIZE);
          LED_B_OFF;
        }
        epd_flushDisplay();
        epd_powerOff();
      } else {
        for (uint8_t i = 0; i < 5; i++) {
          LED_R_ON;
          delay_ms(50);
          LED_R_OFF;
          delay_ms(50);
        }
      }
    }
  }
#endif //RF RX
//////////////////////////////////
//  Inicialización de COBS
#if 1 // Cambiar a 1 para activar el COBS
//////////////////////////////////

  uart_init(); //Inicializa UART para debug
  CobsState __xdata cobsState;
  cobs_init(&cobsState);
  static __xdata uint8_t data[] = "\nHola mundo desde UART!\n¿Como esta el mundo?";
  uint8_t data_length = sizeof(data) - 1; // Restamos 1 para no enviar el caracter nulo

  uint32_t __xdata next = 0;
  while (1) {
    if (cobs_handle(&cobsState)) {
      LED_G_ON;
      cobs_send(cobsState.packet, cobsState.packet_size);
      LED_G_OFF;
    }

    // if (millis() >= next) {
    //   uart_send_str("\n\n\n\nCobs:\n");
    //   cobs_send(data, data_length);
    //   uart_send_str("\n\nUART:\n");
    //   uart_send(data, data_length);
    //   next += 5000;
    //   LED_B_TOGGLE;
    // }
  }
#endif //COBS
}