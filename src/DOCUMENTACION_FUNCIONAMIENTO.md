# Documentación Detallada del Funcionamiento del Firmware

## Resumen General
Este firmware está diseñado para un microcontrolador CC2510FX que controla una pantalla de papel electrónico (e-paper) y maneja comunicación UART con protocolo COBS (Consistent Overhead Byte Stuffing). El sistema funciona como un dispositivo que puede recibir datos por UART, procesarlos y mostrarlos en la pantalla e-paper.

## Estructura del Proyecto
```
src/
├── main.c              # Archivo principal con el bucle principal
├── hal/                # Hardware Abstraction Layer
│   ├── hal.h           # Definiciones comunes del HAL
│   ├── clock.c/h       # Configuración del reloj del sistema
│   ├── time.c/h        # Sistema de tiempo y temporizadores
│   ├── uart.c/h        # Comunicación serie UART
│   ├── led.h           # Control de LEDs
│   └── isr.h           # Declaraciones de rutinas de interrupción
├── display/
│   └── epd.c/h         # Control de pantalla e-paper
└── cobs/
    └── cobs.c/h        # Protocolo COBS para comunicación
```

---

## Análisis Detallado Paso a Paso

### 1. Archivo Principal: `main.c`

#### Líneas 1-11: Inclusión de Headers
```c
#include "display/epd.h"
#include "hal/clock.h"
#include "hal/hal.h"
#include "hal/isr.h"
#include "hal/led.h"
#include "hal/time.h"
#include "hal/uart.h"
#include "cobs/cobs.h"
```
**Ubicación**: `src/main.c:1-11`
**Función**: Incluye todas las librerías necesarias para el funcionamiento del sistema.

#### Línea 12-13: Función Principal
```c
void main(void) {
```
**Ubicación**: `src/main.c:12`
**Función**: Punto de entrada del programa.

#### Líneas 13-18: Inicialización del Sistema
```c
init_clock();           // Línea 13
time_init();           // Línea 14
uart_init();           // Línea 15
HAL_ENABLE_INTERRUPTS(); // Línea 17
LED_INIT;              // Línea 18
```
**Ubicación**: `src/main.c:13-18`
**Función**: 
- `init_clock()`: Configura el reloj del sistema a 26MHz
- `time_init()`: Inicializa el sistema de tiempo con Timer3
- `uart_init()`: Configura la comunicación UART a 115200 baudios
- `HAL_ENABLE_INTERRUPTS()`: Habilita las interrupciones globales
- `LED_INIT`: Configura los pines de los LEDs

#### Líneas 20-24: Secuencia de Test de LEDs
```c
for (uint8_t i = 0; i < 10; i++) {
  LED_TOGGLE;          // Línea 21
  delay_ms(50);        // Línea 22
}
```
**Ubicación**: `src/main.c:20-24`
**Función**: Realiza un test visual parpadeando el LED 10 veces con intervalos de 50ms para indicar que el sistema está funcionando.

#### Línea 25: Inicialización de la Pantalla E-Paper
```c
epd_init();
```
**Ubicación**: `src/main.c:25`
**Función**: Inicializa y configura la pantalla e-paper, incluyendo SPI, pines de control y secuencia de inicialización.

#### Línea 27: Activación del Boost para LEDs
```c
LED_BOOST_ON;
```
**Ubicación**: `src/main.c:27`
**Función**: Activa el circuito boost para alimentar los LEDs con mayor intensidad.

#### Líneas 29-30: Inicialización del Protocolo COBS
```c
CobsState __xdata cobsState;  // Línea 29
cobs_init(&cobsState);       // Línea 30
```
**Ubicación**: `src/main.c:29-30`
**Función**: 
- Declara una estructura de estado para el protocolo COBS en memoria externa
- Inicializa el estado del protocolo COBS

#### Líneas 32-43: Bucle Principal
```c
uint32_t __xdata next = 0;    // Línea 32
while (1) {                   // Línea 33
  if (cobs_handle(&cobsState)) {  // Línea 34
    cobs_send(cobsState.packet, cobsState.packet_size);  // Línea 35
    LED_TOGGLE;              // Línea 36
  }
  // Código comentado para timer periódico
}
```
**Ubicación**: `src/main.c:32-43`
**Función**: 
- Bucle infinito que procesa continuamente los datos recibidos por UART
- `cobs_handle()`: Procesa bytes recibidos y construye paquetes COBS
- Cuando se completa un paquete, lo reenvía y parpadea el LED
- El código comentado (líneas 39-42) sería para un timer periódico

---

### 2. Hardware Abstraction Layer (HAL)

#### 2.1 Archivo `hal/hal.h`
**Ubicación**: `src/hal/hal.h`

##### Líneas 1-13: Protección de Header y Definiciones de Compilación
```c
#ifndef _UTIL_H_
#define _UTIL_H_
#include <stdbool.h>
#ifndef BUILD
#undef SDCC
#undef __SDCC
#define INTERRUPT(name, vector) void name(void)
#define __xdata
#endif
```
**Función**: Define macros para compatibilidad entre diferentes compiladores y entornos de desarrollo.

##### Líneas 15-33: Definiciones del Microcontrolador CC2510FX
```c
#include <cc2510fx.h>
#define BV(x) (1 << (x))
#define HAL_ENABLE_INTERRUPTS() st(EA = 1;)
#define HAL_DISABLE_INTERRUPTS() st(EA = 0;)
#define HAL_INTERRUPTS_ARE_ENABLED() (EA)
```
**Función**: 
- Incluye las definiciones específicas del microcontrolador CC2510FX
- Define macros para manipulación de bits y control de interrupciones
- `BV(x)`: Crea una máscara de bit en la posición x
- Macros para habilitar/deshabilitar interrupciones globales

#### 2.2 Archivo `hal/clock.c` y `hal/clock.h`
**Ubicación**: `src/hal/clock.c` y `src/hal/clock.h`

##### Función `init_clock()` (líneas 3-14 en clock.c)
```c
void init_clock(void) {
  CLKCON = 0x80; // 32 KHz clock osc, 26MHz crystal osc.
  while (!CLOCKSOURCE_XOSC_STABLE()) {
  }
  NOP();
  SLEEP |= 0x04; // power down the unused oscillator
}
```
**Función**: 
- Configura el reloj del sistema para usar el oscilador de cristal de 26MHz
- Espera a que el oscilador se estabilice
- Apaga el oscilador no utilizado para ahorrar energía

#### 2.3 Archivo `hal/time.c` y `hal/time.h`
**Ubicación**: `src/hal/time.c` y `src/hal/time.h`

##### Variable Global de Tiempo (línea 3)
```c
static volatile uint32_t currentTime = 0;
```
**Función**: Almacena el tiempo actual en milisegundos desde el inicio del sistema.

##### Rutina de Interrupción del Timer3 (líneas 5-8)
```c
INTERRUPT(timer3_isr, T3_VECTOR) {
  T3OVFIF = 0; // clear overflow flag
  currentTime += 1;
}
```
**Función**: 
- Se ejecuta cada vez que el Timer3 hace overflow (aproximadamente cada 1ms)
- Limpia la bandera de overflow
- Incrementa el contador de tiempo

##### Función `time_init()` (líneas 10-15)
```c
void time_init() {
  T3CTL = (uint8_t)((0b111 << 5) | BV(1) | BV(3)); // clk/128, modulo mode, overflow interrupt en
  T3CC0 = 203;                                     // ~1ms intrerval
  T3IE = BV(0);                                    // enable timer3 interrupt
  T3CTL |= BV(4);                                  // start Timer 3
}
```
**Función**: 
- Configura el Timer3 para generar interrupciones cada ~1ms
- Habilita las interrupciones del timer
- Inicia el timer

##### Función `millis()` (líneas 17-21)
```c
volatile uint32_t millis() {
  uint32_t value;
  HAL_CRITICAL_STATEMENT(value = currentTime);
  return value;
}
```
**Función**: Retorna el tiempo transcurrido en milisegundos de forma thread-safe.

##### Función `delay_ms()` (líneas 23-27)
```c
void delay_ms(uint16_t milliseconds) {
  uint32_t start = millis();
  while (millis() - start < milliseconds) {
  }
}
```
**Función**: Implementa una función de retardo bloqueante por el número de milisegundos especificado.

#### 2.4 Archivo `hal/uart.c` y `hal/uart.h`
**Ubicación**: `src/hal/uart.c` y `src/hal/uart.h`

##### Buffers de UART (líneas 5-17)
```c
#define UART_TX_BUFFER_SIZE 128
#define UART_RX_BUFFER_SIZE 128
uint8_t __xdata tx_buffer[UART_TX_BUFFER_SIZE];
volatile uint8_t tx_buffer_head = 0;
volatile uint8_t tx_buffer_tail = 0;
volatile bool tx_in_progress = false;
volatile bool tx_buffer_full = false;
uint8_t __xdata rx_buffer[UART_TX_BUFFER_SIZE];
volatile uint8_t rx_buffer_head = 0;
volatile uint8_t rx_buffer_tail = 0;
volatile bool rx_buffer_full = false;
```
**Función**: Define buffers circulares para transmisión y recepción UART con control de cabeza y cola.

##### Rutina de Interrupción de Transmisión (líneas 19-30)
```c
INTERRUPT(uart_tx_isr, UTX1_VECTOR) {
  UTX1IF = 0;
  if (tx_buffer_head == tx_buffer_tail && !tx_buffer_full) {
    tx_in_progress = false;
  } else {
    U1DBUF = tx_buffer[tx_buffer_tail];
    tx_buffer_tail = (tx_buffer_tail + 1) % UART_TX_BUFFER_SIZE;
    tx_buffer_full = false;
  }
}
```
**Función**: 
- Se ejecuta cuando se completa la transmisión de un byte
- Transmite el siguiente byte del buffer si hay datos pendientes
- Actualiza los punteros del buffer circular

##### Rutina de Interrupción de Recepción (líneas 32-43)
```c
INTERRUPT(uart_rx_isr, URX1_VECTOR) {
  URX1IF = 0;
  if (rx_buffer_full) {
    return;
  }
  rx_buffer[rx_buffer_head] = U1DBUF;
  rx_buffer_head = (rx_buffer_head + 1) % UART_RX_BUFFER_SIZE;
  rx_buffer_full = rx_buffer_head == rx_buffer_tail;
}
```
**Función**: 
- Se ejecuta cuando se recibe un byte
- Almacena el byte en el buffer de recepción
- Actualiza los punteros del buffer circular

##### Función `uart_init()` (líneas 45-64)
```c
void uart_init(void) {
  PERCFG &= ~BV(1);  // USART1 use ALT1
  P2DIR = (P2DIR & 0x3F) | BV(6);  // USART1 has priority
  P0SEL |= BV(4) | BV(5);  // configure pin P0_4 (TX) and P0_5 (RX)
  P0DIR |= BV(4);         // make tx pin output
  P0DIR &= ~BV(5);        // make rx pin input
  U1CSR |= BV(7) | BV(6); // uart mode + enable RX
  U1UCR = BV(1);          // high stop bit
  U1BAUD = 34;            // 115200 baud, 26MHz clock
  U1GCR = 12;
  IEN2 |= BV(3); // enable TX interrupt
  URX1IE = 1;    // enable RX interrupt
}
```
**Función**: 
- Configura los pines P0_4 y P0_5 para UART
- Establece la velocidad de comunicación a 115200 baudios
- Habilita las interrupciones de transmisión y recepción

##### Funciones de Envío (líneas 66-93)
```c
void uart_send_byte(uint8_t data) {
  while (tx_buffer_full) {
  }
  tx_buffer[tx_buffer_head] = data;
  HAL_CRITICAL_STATEMENT({
    tx_buffer_head = (tx_buffer_head + 1) % UART_TX_BUFFER_SIZE;
    tx_buffer_full = tx_buffer_head == tx_buffer_tail;
  });
  if (!tx_in_progress) {
    tx_in_progress = true;
    UTX1IF = 1;
  }
}
```
**Función**: Envía un byte por UART usando el buffer de transmisión.

#### 2.5 Archivo `hal/led.h`
**Ubicación**: `src/hal/led.h`

##### Definiciones de Control de LEDs (líneas 1-11)
```c
#define LED_BOOST_ON P2_2 = 1
#define LED_BOOST_OFF P2_2 = 0
#define LED_ON P2_1 = 1
#define LED_OFF P2_1 = 0
#define LED_TOGGLE P2_1 ^= 1
#define LED_INIT            \
  {                         \
    P2DIR |= BV(1) | BV(2); \
    LED_OFF;                \
  }
```
**Función**: 
- Define macros para controlar los LEDs conectados a los pines P2_1 y P2_2
- `LED_BOOST_ON/OFF`: Controla el circuito boost para mayor intensidad
- `LED_ON/OFF/TOGGLE`: Controla el estado del LED principal
- `LED_INIT`: Configura los pines como salidas y apaga el LED

---

### 3. Módulo de Pantalla E-Paper (`display/epd.c` y `display/epd.h`)

#### Definiciones de Pines (líneas 5-21)
```c
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

#define PWR_ON EPD_PWR = 0
#define PWR_OFF EPD_PWR = 1
#define RESET_ON EPD_RESET = 0
#define RESET_OFF EPD_RESET = 1
```
**Ubicación**: `src/display/epd.c:5-21`
**Función**: Define los pines de control de la pantalla e-paper y macros para su control.

#### Configuración de Resolución (líneas 25-27)
```c
#define HRES 152
#define VRES 296
#define BUFFER_SIZE (HRES / 8 * VRES)
```
**Ubicación**: `src/display/epd.c:25-27`
**Función**: Define la resolución de la pantalla (152x296 píxeles) y calcula el tamaño del buffer.

#### Función `epd_waitBusy()` (líneas 32-37)
```c
static void inline epd_waitBusy() {
  do {
  } while (EPD_BUSY == 0);
  delay_ms(200);
}
```
**Ubicación**: `src/display/epd.c:32-37`
**Función**: Espera a que la pantalla termine su operación actual antes de continuar.

#### Función `epd_clearDisplay()` (líneas 39-47)
```c
static void epd_clearDisplay() {
  sendCommand(0x10);
  for (uint16_t i = 0; i < BUFFER_SIZE; i++)
    sendData(0xff);
  sendCommand(0x13);
  for (uint16_t i = 0; i < BUFFER_SIZE; i++)
    sendData(0xff);
}
```
**Ubicación**: `src/display/epd.c:39-47`
**Función**: Borra toda la pantalla enviando datos de 0xFF (blanco) a ambos buffers.

#### Función `epd_init()` (líneas 62-110)
```c
void epd_init() {
  // Configuración SPI
  PERCFG &= ~(0x01);  // USART0 alternative 1 location
  U0CSR = 0;          // SPI mode/master/clear flags
  U0GCR = BV(5) | 17; // SCK-low idle, DATA-1st clock edge, MSB first + baud E
  U0BAUD = 0;         // baud M
  U0CSR |= BV(6);     // enable SPI

  // Configuración de pines
  P0SEL |= BV(3) | BV(5);                        // MISO/MOSI/CLK peripheral functions
  P0DIR |= BV(3) | BV(5) | BV(B_PWR) | BV(B_CS); // MOSI/CLK, PWR/CS output
  P1DIR |= BV(B_DC);
  P1DIR &= ~BV(B_BUSY);
  P2DIR |= BV(B_RESET);

  // Secuencia de inicialización
  PWR_ON;
  delay_ms(1000);
  RESET_ON;
  delay_ms(100);
  RESET_OFF;
  delay_ms(100);

  // Comandos de configuración de la pantalla
  sendCommand(6);
  sendData(0x17);
  sendData(0x17);
  sendData(0x17);

  sendCommand(4);
  epd_waitBusy();

  sendCommand(0);
  sendData(0x0f);
  sendData(0x0d);

  sendCommand(0x61);
  sendData(HRES);
  sendData(VRES >> 8);
  sendData(VRES);

  sendCommand(0x50);
  sendData(0x77);

  epd_clearDisplay();
  epd_refresh();
  epd_sleep();
  PWR_OFF;
}
```
**Ubicación**: `src/display/epd.c:62-110`
**Función**: 
- Configura el SPI para comunicación con la pantalla
- Configura todos los pines de control
- Ejecuta la secuencia de inicialización de la pantalla
- Configura los parámetros de la pantalla (resolución, etc.)
- Borra la pantalla y la pone en modo sleep

#### Funciones de Comunicación SPI (líneas 112-124)
```c
static void inline sendData(uint8_t data) {
  EPD_CS = 0;
  U0DBUF = data;
  while (U0CSR & 0x01) {
  }
  EPD_CS = 1;
}

static void inline sendCommand(uint8_t cmd) {
  EPD_DC = 0;
  sendData(cmd);
  EPD_DC = 1;
}
```
**Ubicación**: `src/display/epd.c:112-124`
**Función**: 
- `sendData()`: Envía datos por SPI con control de CS (Chip Select)
- `sendCommand()`: Envía comandos por SPI (DC=0) seguidos de datos (DC=1)

---

### 4. Módulo COBS (`cobs/cobs.c` y `cobs/cobs.h`)

#### Estructura de Estado COBS (líneas 9-14 en cobs.h)
```c
typedef struct {
  uint8_t packet[MAX_PACKET_SIZE];
  uint8_t packet_size;
  uint8_t packet_index;
  uint8_t block, code;
} CobsState;
```
**Ubicación**: `src/cobs/cobs.h:9-14`
**Función**: Define la estructura que mantiene el estado del protocolo COBS durante la decodificación.

#### Función `cobs_init()` (líneas 3-7 en cobs.c)
```c
void cobs_init(CobsState *state) {
  state->packet_index = 0;
  state->block = 0;
  state->code = 0xFF;
}
```
**Ubicación**: `src/cobs/cobs.c:3-7`
**Función**: Inicializa el estado del protocolo COBS para comenzar a recibir un nuevo paquete.

#### Función `cobs_handle()` (líneas 9-36 en cobs.c)
```c
bool cobs_handle(CobsState *state) {
  uint8_t data;
  if (!uart_read_byte(&data)) {
    return false;
  }

  if (state->packet_index == MAX_PACKET_SIZE) {
    cobs_init(state);
  }

  if (state->block) {
    state->packet[state->packet_index++] = data;
  } else {
    state->block = data;
    if (state->block && (state->code != 0xff))
      state->packet[state->packet_index++] = 0;
    state->code = state->block;
    if (!state->code) {
      state->packet_size = state->packet_index;
      cobs_init(state);
      return true;
    }
  }

  state->block--;
  return false;
}
```
**Ubicación**: `src/cobs/cobs.c:9-36`
**Función**: 
- Procesa bytes recibidos por UART y los decodifica según el protocolo COBS
- Construye el paquete decodificado byte a byte
- Retorna `true` cuando se completa un paquete completo
- Maneja la lógica de decodificación COBS para evitar bytes nulos

#### Función `cobs_send()` (líneas 38-60 en cobs.c)
```c
void cobs_send(const uint8_t *data, uint8_t length) {
  uint8_t index = 0;
  uint8_t code = 1;
  while (index < length) {
    uint8_t value = data[index];
    if (code == 1) {
      for (uint8_t i = index; i < length && data[i]; i++) {
        code++;
      }
      uart_send_byte(code);
    } else {
      code--;
    }
    if (value) {
      uart_send_byte(value);
    }
    index++;
  }
  if (code == 1) {
    uart_send_byte(code);
  }
  uart_send_byte(0);
}
```
**Ubicación**: `src/cobs/cobs.c:38-60`
**Función**: 
- Codifica datos usando el protocolo COBS antes de enviarlos por UART
- Inserta códigos de longitud para evitar bytes nulos
- Termina el paquete con un byte nulo (0)

---

## Flujo de Funcionamiento Completo

### Secuencia de Inicialización:
1. **Configuración del Reloj** (`init_clock()`): Establece el sistema a 26MHz
2. **Inicialización del Tiempo** (`time_init()`): Configura Timer3 para medición de tiempo
3. **Configuración UART** (`uart_init()`): Establece comunicación serie a 115200 baudios
4. **Habilitación de Interrupciones**: Activa el sistema de interrupciones
5. **Inicialización de LEDs**: Configura pines y realiza test visual
6. **Inicialización E-Paper** (`epd_init()`): Configura SPI y pantalla
7. **Inicialización COBS**: Prepara el protocolo de comunicación

### Bucle Principal:
1. **Recepción de Datos**: `cobs_handle()` procesa bytes recibidos por UART
2. **Decodificación COBS**: Construye paquetes decodificados
3. **Reenvío de Paquetes**: Cuando se completa un paquete, lo reenvía usando `cobs_send()`
4. **Indicación Visual**: Parpadea LED para indicar actividad

### Características del Sistema:
- **Comunicación Bidireccional**: Recibe datos por UART y los reenvía
- **Protocolo COBS**: Maneja datos binarios de forma segura
- **Pantalla E-Paper**: Lista para mostrar información (aunque no se usa en el bucle principal)
- **Sistema de Tiempo**: Medición precisa de tiempo con interrupciones
- **Control de LEDs**: Indicación visual del estado del sistema

Este firmware actúa como un dispositivo de retransmisión que recibe datos codificados con COBS por UART, los decodifica y los reenvía, proporcionando una interfaz robusta para comunicación serie con pantalla e-paper integrada.
