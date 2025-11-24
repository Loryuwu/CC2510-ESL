# Documentación del Módulo EPD (Electronic Paper Display)

Este documento detalla el funcionamiento de las macros y funciones contenidas en el módulo `epd` (`epd.c` y `epd.h`), encargado del control de la pantalla de tinta electrónica.

## Macros (`epd.h`)

Las macros en este archivo definen la configuración de hardware, pines y parámetros de la pantalla.

### Definición de Pines y Hardware
Estas macros mapean nombres abstractos a los registros físicos del microcontrolador CC2510.

*   **`B_PWR`, `B_CS`, `B_DC`, `B_BUSY`, `B_RESET`**: Definen la posición del bit (0-7) en el puerto correspondiente para cada señal de control.
*   **`EPD_PWR` (P0_0)**: Pin de alimentación de la pantalla.
*   **`EPD_CS` (P0_1)**: Chip Select (Selección de chip) para la comunicación SPI.
*   **`EPD_DC` (P1_2)**: Data/Command. Determina si lo que se envía es un comando o un dato.
*   **`EPD_BUSY` (P1_3)**: Pin de entrada que indica si la pantalla está ocupada procesando una operación.
*   **`EPD_RESET` (P2_0)**: Pin de reinicio (Reset) por hardware.

### Control de Estado (Nivel Bajo)
Estas macros simplifican el cambio de estado de los pines de control.

*   **`EPD_PWR_ON` / `EPD_PWR_OFF`**: Enciende (`0`) o apaga (`1`) la alimentación de la pantalla. *Nota: Parece usar lógica activa baja o controlar un transistor PNP/MOSFET canal P.*
*   **`EPD_RESET_ON` / `EPD_RESET_OFF`**: Activa (`0`) o desactiva (`1`) la señal de Reset. Es activo bajo.
*   **`EPD_SELECT` / `EPD_DESELECT`**: Activa (`0`) o desactiva (`1`) la comunicación SPI con la pantalla.
*   **`EPD_CMDMODE`**: Pone el pin DC en bajo (`0`) para indicar que se enviará un **comando**.
*   **`EPD_DATAMODE`**: Pone el pin DC en alto (`1`) para indicar que se enviarán **datos**.

### Configuración de Pantalla
*   **`EPD_SIZE`**: Define el tamaño físico de la pantalla (ej. 266 para 2.66 pulgadas).
*   **`HRES` / `VRES`**: Resolución horizontal y vertical, definidas automáticamente según `EPD_SIZE`.
*   **`BUFFER_SIZE`**: Calcula el tamaño en bytes necesario para almacenar una imagen completa (`HRES / 8 * VRES`).

---

## Funciones (`epd.c`)

### Funciones de Inicialización y Control

#### `void epd_init()`
*   **Qué hace:** Inicializa el periférico SPI (USART0) del CC2510 y configura los pines GPIO necesarios. Luego, enciende la pantalla, realiza un reinicio por hardware y software, y envía configuraciones iniciales (como el sensor de temperatura).
*   **Detalle:** Configura la velocidad de baudios, modo SPI (Master), y dirección de los pines. Es la primera función que debe llamarse.

#### `void epdReset(uint32_t ms1, uint32_t ms2, uint32_t ms3, uint32_t ms4, uint32_t ms5)`
*   **Qué hace:** Realiza una secuencia de reinicio por hardware toggling el pin `RESET`.
*   **Detalle:** Permite especificar los tiempos de espera (en milisegundos) entre cada cambio de estado del pin RESET. Es útil para cumplir con los tiempos específicos requeridos por el controlador de la pantalla al encenderse.

#### `void softReset()`
*   **Qué hace:** Envía un comando de reinicio por software (`0x00`) al controlador de la pantalla.
*   **Diferencia con `epdReset`:** `epdReset` usa el pin físico eléctrico. `softReset` envía una instrucción digital. Generalmente se usan ambos durante la inicialización.

#### `void epd_waitBusy()`
*   **Qué hace:** Bloquea la ejecución del programa hasta que la pantalla deje de estar ocupada.
*   **Detalle:** Monitorea el pin `EPD_BUSY`. Incluye un pequeño retardo de seguridad al final. Es crucial llamar a esto después de comandos que toman tiempo (como refrescar la pantalla) para no enviar datos antes de que la pantalla esté lista.

#### `void DCDC_powerOn()`
*   **Qué hace:** Envía el comando (`0x04`) para encender el convertidor DC/DC interno de la pantalla.
*   **Detalle:** Necesario para generar los altos voltajes requeridos para mover la tinta electrónica.

#### `void powerOff()`
*   **Qué hace:** Apaga el convertidor DC/DC y pone la pantalla y los pines del microcontrolador en modo de bajo consumo/seguro.
*   **Detalle:** Se debe llamar después de terminar de actualizar la pantalla para ahorrar energía y proteger el display.

### Funciones de Dibujo y Actualización

#### `void globalUpdate(const uint8_t * data1s, const uint8_t * data2s)`
*   **Qué hace:** Envía dos buffers de imagen completos a la pantalla y luego dispara la actualización visual.
*   **Detalle:** Las pantallas E-Ink suelen tener dos buffers de memoria (ej. para el estado actual y el siguiente, o para color negro y rojo). Esta función llena ambos y llama a `flushDisplay`.

#### `void epd_clearDisplay()`
*   **Qué hace:** Borra la pantalla (la pone en blanco/limpia).
*   **Detalle:** Envía bytes `0x00` a toda la memoria de la pantalla usando `sendColor` y luego actualiza. Es una forma rápida de limpiar sin necesitar un buffer de imagen en la RAM del microcontrolador.

#### `void flushDisplay()`
*   **Qué hace:** Inicia la secuencia de refresco de la pantalla (`0x12`).
*   **Detalle:** Esto es lo que realmente hace que la imagen cambie físicamente. Antes de esto, solo se están escribiendo datos en la memoria interna del controlador. Enciende el DC/DC automáticamente.

### Funciones de Comunicación (Bajo Nivel)

#### `void sendIndexData(uint8_t index, const uint8_t *data, uint32_t len)`
*   **Qué hace:** Envía un comando (`index`) seguido de una secuencia de bytes de datos (`data`).
*   **Uso:** Para enviar configuraciones o bloques de imagen que ya están en un array.
*   **Diferencia:** Maneja automáticamente el cambio entre modo Comando y modo Datos, y el Chip Select.

#### `void sendColor(uint8_t index, const uint8_t data, uint32_t len)`
*   **Qué hace:** Envía un comando (`index`) y luego envía **el mismo byte** (`data`) repetido `len` veces.
*   **Diferencia con `sendIndexData`:** `sendIndexData` recorre un array de datos distintos. `sendColor` repite un solo valor. Es muy eficiente para llenar la pantalla de un solo color (ej. todo blanco o todo negro) sin gastar RAM del microcontrolador en un buffer.

#### `void sendCommand(uint8_t cmd)`
*   **Qué hace:** Envía un único byte interpretado como comando.
*   **Detalle:** Pone `EPD_DC` en bajo antes de enviar.

#### `void sendData(uint8_t data)`
*   **Qué hace:** Envía un único byte interpretado como dato (parámetro de un comando).
*   **Detalle:** Pone `EPD_DC` en alto antes de enviar.
