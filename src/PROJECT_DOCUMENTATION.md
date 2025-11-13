## Documentación del proyecto (carpeta `src`)

Esta documentación cubre los archivos dentro de la carpeta `src` del firmware. Está escrita en español e incluye: descripción general, módulos, APIs públicas, instrucciones de compilación/flash, notas de diseño y pasos recomendados.

## Resumen general

Este firmware está pensado para un microcontrolador CC2510FX que controla una pantalla e-paper (EPD) y realiza comunicación UART usando COBS. Su comportamiento principal actual:
- Inicializa reloj, timers, UART y la pantalla.
- Soporta recepción y envío de paquetes COBS por UART.
- Tiene buffers de imagen precargados (en `image_data/`) para actualizar la pantalla.

El flujo típico de inicialización: `init_clock()` -> `time_init()` -> `uart_init()` -> habilitar interrupciones -> `epd_init()` -> (usar `globalUpdate()` o `epd_clearDisplay()`).

## Estructura de la carpeta `src`

- `main.c` : punto de entrada. Inicialización y bucle principal.
- `hal/` : Hardware Abstraction Layer (reloj, timers, UART, macros de interrupción, LEDs).
- `display/` : controladores de EPD (SPI, secuencia de init, envío de comandos/datos, refresco, sleep, power).
- `cobs/` : implementación del protocolo COBS (decodificación y codificación para envío/recepción UART).
- `image_data/` : buffers de imagen en formato C (black/red) para diferentes tamaños de EPD.
- `rf/` : módulo de radio RF del CC2510 (configuración, TX/RX de paquetes, gestión RSSI/LQI).

## Módulos y API pública (resumen)

- main.c
  - main(void): realiza inicializaciones y (según rama) procesa paquetes COBS en un bucle.

- rf/rf.h
  - void rf_init(void): inicializa el módulo de radio (registros, interrupciones)
  - void rf_set_channel(uint8_t channel): configura canal RF (0-255)
  - void rf_set_power(uint8_t power): configura potencia TX (0-255)
  - bool rf_send_packet(const uint8_t *data, uint8_t length): envía paquete
  - uint8_t rf_receive_packet(uint8_t *buffer): recibe paquete si disponible
  - int8_t rf_get_rssi(void): obtiene RSSI de último paquete
  - uint8_t rf_get_lqi(void): obtiene LQI de último paquete
  - rf_state_t rf_get_state(void): obtiene estado actual (IDLE/RX/TX)

- hal/hal.h
  - Macros: `HAL_ENABLE_INTERRUPTS()`, `HAL_DISABLE_INTERRUPTS()`, `HAL_CRITICAL_STATEMENT(...)`.
  - Tip: estas macros adaptan el código para SDCC/compiladores y encapsulan la crítica de interrupciones.

- hal/clock.c
  - void init_clock(void): configura CLKCON para usar cristal (26MHz) y espera estabilidad.

- hal/time.c / time.h
  - void time_init(void): configura Timer3 para tick ~1ms y habilita ISR.
  - uint32_t millis(void): tiempo transcurrido en ms (thread-safe con macros HAL_CRITICAL_STATEMENT).
  - void delay_ms(uint16_t ms): retardo bloqueante.

- hal/uart.c / uart.h
  - void uart_init(void): configura USART1 para 115200 baudios, pines y habilita interrupciones RX/TX.
  - void uart_send_byte(uint8_t b): coloca byte en buffer TX (circular).
  - void uart_send(const uint8_t *data, size_t len)
  - uint8_t uart_available(void): bytes en buffer RX.
  - bool uart_read_byte(uint8_t *out): leer un byte (si disponible).
  - Interrupciones: `uart_tx_isr` y `uart_rx_isr` gestionan buffers circulares.

- hal/led.h
  - Macros para controlar LEDs y boost (`LED_ON`, `LED_TOGGLE`, `LED_BOOST_ON`, etc.) y `LED_INIT`.

- display/epd.c / epd.h
  - void epd_init(void): configura SPI (USART0 en modo SPI), pines y la secuencia de inicialización de la EPD.
  - void globalUpdate(const uint8_t *data1, const uint8_t *data2): envía los buffers y refresca la pantalla.
  - void epd_clearDisplay(void): limpia los buffers de la pantalla.
  - void powerOff(void): apaga la pantalla / DC-DC cuando ya no se usa.
  - Constantes: `EPD_SIZE` (213/266/417), `HRES`, `VRES`, `BUFFER_SIZE`.

- cobs/cobs.c / cobs.h
  - typedef CobsState: estructura con `packet[]`, `packet_size`, `packet_index`, `block`, `code`.
  - void cobs_init(CobsState *s)
  - bool cobs_handle(CobsState *s): procesa bytes recibidos (usa `uart_read_byte`) y devuelve true cuando un paquete quedó completo (decodificado).
  - void cobs_send(const uint8_t *data, uint8_t len): codifica y envía por UART usando `uart_send_byte`.

- image_data/
  - Buffers C integrados para cada tamaño de EPD: `image_*_blackBuffer`, `image_*_redBuffer`.
  - Macros `lory_blackBuffer` y `lory_redBuffer` apuntan al buffer adecuado según `EPD_SIZE`.

## Constantes de configuración y variantes

- Tamaño de EPD: `EPD_SIZE` en `display/epd.h` determina HRES/VRES y qué buffers de `image_data` son usados.
- UART: configurada para 115200 baudios (26MHz), los parámetros están en `uart_init()` (U1BAUD / U1GCR).
- Buffers TX/RX: tamaño 128 bytes (configurable en `uart.c` macros `UART_TX_BUFFER_SIZE` y `UART_RX_BUFFER_SIZE`).

## Protocolo de comunicación

- El firmware utiliza COBS para encapsular mensajes sin bytes nulos. `cobs_handle()` consume bytes del buffer RX y arma paquetes decodificados; cuando un paquete se completa, `cobs_handle()` devuelve true y el paquete decodificado está en `CobsState.packet` con length `packet_size`.
- Para enviar, usar `cobs_send()` que codifica y termina con un `0`.

## Instrucciones de compilación y flash (local)

Requisitos mínimos: SDCC (u otro toolchain para CC2510FX) y `make`.

1) Limpiar:

```powershell
make clean
```

2) Compilar:

```powershell
make
```

3) Flashear: en este repositorio hay un script/programador en `../programmer` (Node/ts-node). Hay una tarea: `flash` que ejecuta:

```powershell
cd ..\programmer; node -r ts-node/register src/index.ts ..\firmware\firmware.hex /dev/ttyUSB1
```

Nota: el dispositivo y puerto serie pueden diferir en Windows; ajustar `/dev/ttyUSB1` al puerto correcto (ej. `COM3`) y comandos según la herramienta de programado que tengas.

## Cómo añadir o actualizar una imagen para EPD

- Hay un script Python en `image_converter/image_converter.py` que genera los arrays C usados en `image_data/`.
- Flujo recomendado: convertir imagen -> copiar arrays resultantes a `src/image_data/` o generar un nuevo archivo C -> compilar.

## Contratos, inputs/outputs y casos de error (mini-contrato)

- RF API
  - Input: datos a transmitir o buffer para recepción.
  - Output: estado de envío (bool), bytes recibidos, RSSI/LQI.
  - Error modes: buffer overflow en RX descarta paquetes. CRC incorrecto descarta paquete.
  - ISR: rf_isr() maneja RX/TX completo y actualiza estado.

- UART API
  - Input: bytes desde hardware UART.
  - Output: funciones de lectura/escritura no bloqueantes (leer devuelve booleano si hay o no dato).
  - Error modes: buffer overflow en RX descarta bytes (la ISR comprueba `rx_buffer_full`). Considerar ampliar buffer o notificar error.

- COBS
  - Input: flujo de bytes UART.
  - Output: paquetes decodificados en `CobsState.packet`.
  - Error modes: paquetes demasiado largos reinician estado (`MAX_PACKET_SIZE`).

- EPD
  - Input: pointers a buffers de tamaño `BUFFER_SIZE` y comandos de control.
  - Output: pantalla actualizada después de `globalUpdate()`.
  - Error modes: busy flag (EPD_BUSY) se debe respetar; `epd_waitBusy()` incluye retardos.

## Edge cases y recomendaciones

- RF packet overflow: paquetes mayores que RF_MAX_PACKET_SIZE son descartados.
- RF collision/interference: verificar RSSI antes de TX, usar rf_get_rssi() para diagnóstico.
- RF channel overlap: elegir canales con suficiente separación (>5) para evitar interferencia.

- UART RX overflow: actualmente descarta bytes si buffer lleno. Añadir contador de errores/log para diagnósticos.
- COBS paquete demasiado largo: cobs_init() reinicia el estado; considerar notificar al sistema (LED/error) o incrementar `MAX_PACKET_SIZE` si el protocolo lo permite.
- Energía: `epd_init()` y `powerOff()` controlan DC-DC; en dispositivos con batería, asegurar secuencia correcta antes de apagar.
- Concurrencia: `millis()` y los contadores son protegidos por macros críticas. Mantener esa convención al añadir código que acceda a variables compartidas.

## Sugerencias de mejoras (próximos pasos)

1. Añadir README corto en raíz de `src/` con resumen y cómo compilar (ya incluido en este archivo, pero un README minimal sería útil).
2. Añadir tests unitarios/hosted para la implementación COBS (pequeña biblioteca en C con harness que corra en PC) para validar codificación/decodificación.
3. Documentar el formato exacto de paquetes (payload) que el dispositivo espera dentro de COBS (si hay un protocolo superior, ej. JSON, TLV, etc.).
4. Registrar/contar errores UART y COBS para diagnóstico (estadísticas en memoria circular o exposición por comando de debug).
5. Automatizar el proceso de conversión de imagen con un pequeño script que escriba directamente en `src/image_data` y actualice `EPD_SIZE` si es necesario.

## Archivos importantes (ubicaciones)

- `src/main.c` — inicialización y bucle.
- `src/hal/*` — HAL: `clock.c`, `time.c`, `uart.c`, `led.h`, `hal.h`.
- `src/rf/*` — Módulo RF: `rf.c`, `rf.h` para comunicación radio.
- `src/display/epd.c` y `src/display/epd.h` — driver de EPD.
- `src/cobs/*` — cobs encoder/decoder.
- `src/image_data/*` — buffers C con imagen.
- `image_converter/` — herramientas para convertir imágenes.

## Verificación rápida (manual)

1. Ejecutar `make` y observar errores de compilación.
2. Conectar el programador y ejecutar la tarea `flash` (ajustar puerto en Windows).
3. Si la pantalla no se actualiza, revisar `EPD_BUSY` y secuencia de reset en `epd_init()`.

## Resumen de cambios hechos

- Documento creado: `src/PROJECT_DOCUMENTATION.md` — documentación general del `src` (este archivo).

## Próximos pasos (sugeridos al mantener o mejorar el repositorio)

- Validar build local (ejecutar `make`).
- Ejecutar pruebas de COBS (pequeño harness) y verificar buffers UART.
- Automatizar generación de imágenes y añadir guía para flasheo en Windows (puerto COM).

---

Si quieres, ahora puedo:
- Ejecutar `make` y reportar el resultado (compilar el firmware). 
- Añadir un README.md minimal en la raíz de `src` con un tl;dr.
- Generar un archivo de referencia de la API (tabla de funciones) extraído automáticamente.

Indica qué prefieres que haga a continuación.
