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

#include "nfc/nfc.h"
#include "nfc/i2c.h"

#include "flash/flash.h"

// Global buffer for flash operations to save stack
#define CHUNK_SIZE 32
static __xdata uint8_t buffer[CHUNK_SIZE];
static __xdata uint32_t g_addr;
static __xdata uint32_t g_i;
static __xdata uint16_t g_len;

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

#if 1 //test display
  LED_B_ON; 
  epd_init();
  
  // 1. Mostrar imagen normal
  epd_setBWMode(0); // Modo normal
  // epd_sendIndexData(0x10, image1, BUFFER_SIZE);
  epd_sendColor(0x10, 0x55, BUFFER_SIZE);
  epd_sendColor(0x13, 0x00, BUFFER_SIZE);
  epd_flushDisplay();
  LED_B_OFF;
  
  delay_ms(2000); // Pausa para ver la imagen
  
  // 2. Probar refresco parcial
  LED_B_ON;
  epd_setBWMode(1); // Modo B/N para refresco rápido
  epd_drawBlackRect(32, 32, 64, 64); // Cuadro negro de prueba (alineado a 8)
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
//  rf_init();
  LED_BOOST_OFF;
  spi_flash_init();

  uint16_t flash_id = spi_flash_read_id();

  // Verificamos ID (Winbond es 0xEF)
  if ((flash_id >> 8) == 0xEF) {
      // Indicación visual breve de éxito
      LED_BOOST_ON;
      LED_G_ON; delay_ms(100); LED_G_OFF;
      LED_BOOST_OFF; // Apagamos boost para evitar flicker en LEDs durante lectura

      // Inicializar EPD
      epd_init();

      // -------------- CANAL B/N (0x10) DESDE FLASH --------------
      // Leemos de Flash en chunks y enviamos al EPD
      epd_stream_start(0x10);

      g_addr = 0; // Dirección inicio imagen en Flash
      // uint32_t total_bytes = BUFFER_SIZE;

      for (g_i = 0; g_i < BUFFER_SIZE; g_i += CHUNK_SIZE) {
          g_len = CHUNK_SIZE;
          if ((g_i + g_len) > BUFFER_SIZE) {
              g_len = BUFFER_SIZE - g_i;
          }

          // 1. Leer de Flash (USART1)
          spi_flash_read(g_addr, buffer, g_len);
          g_addr += g_len;

          // Invertir colores (Prueba)
          //for(uint8_t k=0; k<g_len; k++) buffer[k] = ~buffer[k];

          // 2. Escribir a EPD (USART0) - Sin cerrar la transacción del EPD
          epd_stream_data(buffer, g_len);
      }
      epd_stream_end(); // Fin de comando 0x10

      // -------------- CANAL ROJO (0x13) DESDE FLASH --------------
      epd_stream_start(0x13);
      
      // Continuamos leyendo desde la Flash donde quedamos (o forzamos inicio de Red)
      g_addr = BUFFER_SIZE; 

      for (g_i = 0; g_i < BUFFER_SIZE; g_i += CHUNK_SIZE) {
          g_len = CHUNK_SIZE;
          if ((g_i + g_len) > BUFFER_SIZE) {
              g_len = BUFFER_SIZE - g_i;
          }

          // 1. Leer de Flash (USART1)
          spi_flash_read(g_addr, buffer, g_len);
          g_addr += g_len;
          
          // Invertir colores si es necesario (comentado por defecto)
          // for(uint8_t k=0; k<g_len; k++) buffer[k] = ~buffer[k];

          // 2. Escribir a EPD (USART0)
          epd_stream_data(buffer, g_len);
      }
      epd_stream_end(); // Fin de comando 0x13

      // Refrescar y Apagar
      epd_flushDisplay();
      epd_powerOff();

      // Desactivar Flash y Pines
      spi_flash_disable();
      FLASH_POWER_OFF;
      
      // Éxito Final
      LED_BOOST_ON;
      while(1) {
         LED_G_ON; delay_ms(200); LED_G_OFF; delay_ms(2000);
      }

  } else {
      // Error ID
      FLASH_POWER_OFF;
      LED_BOOST_ON;
      while(1) {
        LED_R_TOGGLE; delay_ms(100);
      }
  }

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
#if 0 // Cambiar a 1 para activar el COBS
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
      delay_ms(500);
      cobs_send(cobsState.packet, cobsState.packet_size);
      LED_G_OFF;
    }

    if (millis() >= next) {
      cobs_send_str("\n[Cobs]:");
      cobs_send(data, data_length);
      uart_send_str("\n[UART]:");
      uart_send(data, data_length);
      next += 5000;
      LED_B_TOGGLE;
    }
  }
#endif //COBS
//////////////////////////////////
//  Test de UART Texto Plano
#if 0 // Cambiar a 1 para activar test
//////////////////////////////////

  uart_init();
  
  uart_send_str("\r\nSistema iniciado: Esperando texto UART...\r\n");

  uint8_t rx_byte;
  while (1) {
    if (uart_available()) {
        if (uart_read_byte(&rx_byte)) {
            // Echo del caracter recibido
            uart_send_byte(rx_byte);
            
            // Si recibimos Enter, hacemos parpadear el LED
            if (rx_byte == '\r') {
                uart_send_byte('\n'); // Nueva linea visual
                for (uint8_t i = 0; i < 10; i++) {
                  LED_G_TOGGLE;
                  delay_ms(50);
                }
            }
        }
    }
  }
#endif //uart texto plano
//////////////////////////////////
//  Test de NFC encontrar direccion
#if 0 // Cambiar a 1 para activar test
//////////////////////////////////
//  Test de NFC
  nfc_init();
  P1_0 = 1;

  // I2C Scanner Loop
  // Scans addresses 1 to 127
  
  uint8_t found_addr = nfc_scan();
   
  
  if (found_addr != 0) {
      // FOUND!
      while(1) {
         // Blink High Nibble (Upper 4 bits) - RED
         uint8_t High = (found_addr >> 4);
         if (High == 0) {
             LED_B_ON; delay_ms(500); LED_B_OFF; // Long Blue = 0
         } else {
             for(uint8_t k=0; k<High; k++) {
                 LED_R_ON; delay_ms(300); LED_R_OFF; delay_ms(300);
             }
         }
         
         delay_ms(1000); // Pause between nibbles
         
         // Blink Low Nibble (Lower 4 bits) - GREEN
         uint8_t Low = (found_addr & 0x0F);
         if (Low == 0) {
             LED_B_ON; delay_ms(500); LED_B_OFF; // Long Blue = 0
         } else {
             for(uint8_t k=0; k<Low; k++) {
                 LED_G_ON; delay_ms(300); LED_G_OFF; delay_ms(300);
             }
         }
         
         delay_ms(3000); // Long Pause before repeating
      }
  } else {
      // Nothing found
      while(1) {
          LED_R_ON; delay_ms(100); LED_R_OFF; delay_ms(100);
      }
  }

  while (1);
#endif //nfc
//////////////////////////////////
//  Test de NFC (Erase & Write)
#if 0
//////////////////////////////////
//  Run Erase & Write Test
  nfc_init();

  // 1. Safe Erase (User Memory Only: I2C Page 1..0x39)
  // Blink Blue while erasing
  LED_B_ON;
  nfc_erase_all(); // NOW Safe!
  LED_B_OFF;
  delay_ms(500);

  // 2. Write "HelOwOrld ^~^" to I2C Page 1 (NFC Block 4 - First User Page)
  static __xdata uint8_t hello_msg[16] = "HelOwOrld ^~^";
  
  // Write to Page 1
  if (nfc_write_page(1, hello_msg)) {
      // Write OK, Verify?
      static __xdata uint8_t read_back[16];
      nfc_read_page(1, read_back);
      
      // Check first char matches 'H'
      if (read_back[0] == 'H') {
          // SUCCESS! Green Heartbeat
          while(1) {
              LED_G_ON; delay_ms(100); LED_G_OFF; delay_ms(100);
              LED_G_ON; delay_ms(100); LED_G_OFF; delay_ms(800);
          }
      } else {
           // Write Ack'd but Read mismatch -> Green/Red Alternating
           while(1) {
              LED_G_ON; delay_ms(200); LED_G_OFF;
              LED_R_ON; delay_ms(200); LED_R_OFF;
          }
      }
  } else {
      // Write NACK -> RED Fast
      while(1) {
          LED_R_ON; delay_ms(50); LED_R_OFF; delay_ms(50);
      }
  }

  while (1);
#endif //nfc

}