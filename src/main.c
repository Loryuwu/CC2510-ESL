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
  //uart_init(); //Inicializa UART para debug
  
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
  sendIndexData(0x10, image4 , BUFFER_SIZE);
  sendColor(0x13, 0x00, BUFFER_SIZE);
  flushDisplay();
  LED_B_OFF;
  powerOff();
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
            sendIndexData(0x10, image1, BUFFER_SIZE);
            sendColor(0x13, 0x00, BUFFER_SIZE); // Asumiendo que image1 es B/N y el canal rojo va vacío
            break;
        case 1:
            sendIndexData(0x10, image2, BUFFER_SIZE);
            sendColor(0x13, 0x00, BUFFER_SIZE);
            break;
        case 2:
            sendIndexData(0x10, image3, BUFFER_SIZE);
            sendColor(0x13, 0x00, BUFFER_SIZE);
            break;
    }

    // 3. Refrescar pantalla
    flushDisplay();
    LED_B_OFF;
    // 4. Apagar pantalla (Deep Sleep del EPD)
    powerOff();

    // 5. Alternar imagen para la próxima vez
    current_image += 1;
    if (current_image >= 3) {
      current_image = 0;
    }

    // 6. Dormir el MCU por x horas (en alguna parte hay un error, pero 1 segundo equivale a medio minuto realmente)
    deep_sleep_seconds(120*6); //120 equivale a 1 hora, multiplicamos por x cantidad de horas (en este caso 6)
    // delay_ms(10000);
  }


#endif //EPD
////////////////////////////////
//  Inicialización de RF TX
#if 1 // Cambiar a 1 para activar el test de RF
////////////////////////////////
  delay_ms(500);
  LED_B_ON;
  rf_init();
  
  static __xdata uint8_t data[] = "Hola mundo desde RF!";
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
#if 1 // Cambiar a 1 para activar el test de RF
////////////////////////////////
  rf_init();
  
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
              LED_G_ON;  // Si datos recividos comienza con "LED" Encender LED verde
              delay_ms(5000);
              LED_G_OFF;
          }
          else {
              LED_R_ON;  // Si datos recividos no comienza con "LED" Encender LED rojo
              delay_ms(5000);
              LED_R_OFF;
          }
      }
  }
#endif //RF RX
//////////////////////////////////
//  Inicialización de COBS
#if 0 // Cambiar a 1 para activar el COBS
//////////////////////////////////
  CobsState __xdata cobsState;
  cobs_init(&cobsState);

  uint32_t __xdata next = 0;
  while (1) {
    if (cobs_handle(&cobsState)) {
      cobs_send(cobsState.packet, cobsState.packet_size);
      LED_TOGGLE;
    }

    if (millis() >= next) {
      next += 1000;
      LED_TOGGLE;
    }
  }
#endif //COBS
}