# 03. Módulo HAL (Hardware Abstraction Layer)

La Capa de Abstracción de Hardware (HAL) aísla la lógica de la aplicación de los detalles específicos del hardware del CC2510. Se encuentra en el directorio `src/hal/`.

## 1. Gestión de Reloj (`clock.c`, `clock.h`)
El CC2510 dispone de dos osciladores de sistema:
*   **RC OSC (16 MHz / 13 MHz):** Oscilador interno, bajo consumo pero baja precisión. Es el activo por defecto al encender.
*   **XOSC (24 MHz - 27 MHz):** Oscilador de cristal externo. Es **obligatorio** para el funcionamiento correcto del Radio (RF) y para obtener baudrates precisos en UART.

### Funciones Clave:
*   `init_clock()`: Realiza la secuencia para encender el oscilador de cristal externo, esperar a que se estabilice y cambiar el reloj del sistema a esta fuente.
    1.  Enciende el XOSC (`SLEEP &= ~0x04`).
    2.  Espera a que el bit de estabilidad se active (`while(!(SLEEP & 0x40))`).
    3.  Cambia la fuente de reloj (`CLKCON = (CLKCON & ~0x40) | 0x00`).
    4.  Apaga el oscilador RC para ahorrar energía (`SLEEP |= 0x04`).

## 2. Gestión de Tiempo (`time.c`, `time.h`)
Proporciona utilidades para medir el paso del tiempo y generar retardos. Utiliza el **Timer 1** o **Timer 3** (dependiendo de la implementación específica) o bucles calibrados.

### Funciones Clave:
*   `time_init()`: Configura el Timer para generar interrupciones periódicas (tick) o prepara el sistema para contar milisegundos.
*   `delay_ms(uint16_t ms)`: Genera un retardo bloqueante de `ms` milisegundos. Esencial para secuencias de inicialización de hardware.
*   `millis()`: Retorna el número de milisegundos transcurridos desde el inicio del sistema (útil para temporización no bloqueante).
*   `sleep_ms(uint32_t ms)`: Intenta poner el microcontrolador en modo de bajo consumo (PM1/PM2) durante un tiempo determinado.

## 3. Control de LEDs (`led.c`, `led.h`)
Maneja los pines GPIO conectados a los LEDs de la etiqueta.
*   **Pines Típicos:**
    *   **Rojo:** P1_5 (Compartido con SPI CLK)
    *   **Verde:** P1_7 (Compartido con SPI MISO)
    *   **Azul:** P1_6 (Compartido con SPI MOSI)
    *   **Blanco:** P2_1 (Dedicado)

> **⚠️ Nota Importante:** Debido a que los LEDs RGB comparten pines con el bus SPI de la memoria Flash, no se pueden usar simultáneamente con operaciones de Flash. El LED Blanco suele ser seguro de usar en cualquier momento.

### Macros:
*   `LED_INIT`: Configura los pines como salida.
*   `LED_ON` / `LED_OFF` / `LED_TOGGLE`: Controlan el LED Blanco.
*   `LED_R_ON`, `LED_G_ON`, `LED_B_ON`: Controlan los LEDs RGB (con la precaución mencionada).

## 4. UART (`uart.c`, `uart.h`)
El CC2510 tiene dos interfaces USART (USART0 y USART1) que pueden funcionar como UART o SPI.
Este módulo configura una de ellas (usualmente USART0 Alt 1 o Alt 2) para comunicación serial asíncrona.

### Configuración Típica:
*   **Baudrate:** 115200 (depende de `U0BAUD` y `U0GCR`).
*   **Pines:** P0_2 (RX) y P0_3 (TX) en Alt 1.

### Funciones Clave:
*   `uart_init()`: Configura los registros `U0CSR`, `U0GCR`, `U0BAUD` y los pines `P0SEL`.
*   `uart_write(uint8_t byte)`: Envía un byte y espera a que se complete la transmisión.
*   `uart_read(uint8_t *byte)`: Verifica si hay datos recibidos y los lee.

## 5. Interrupciones (`isr.h`)
Define las macros y prototipos para manejar las interrupciones del sistema (RF, DMA, Timers, I/O).
*   `HAL_ENABLE_INTERRUPTS()`: Habilita interrupciones globales (`EA = 1`).
*   `HAL_DISABLE_INTERRUPTS()`: Deshabilita interrupciones globales (`EA = 0`).
