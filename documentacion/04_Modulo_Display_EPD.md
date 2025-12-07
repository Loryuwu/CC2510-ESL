# 04. Módulo Display EPD (Electronic Paper Display)

Este módulo (`src/display/epd.c`) controla la pantalla de tinta electrónica. Estas pantallas requieren una secuencia específica de voltajes y comandos para actualizarse correctamente sin dañarse.

## Hardware
*   **Interfaz:** SPI (Serial Peripheral Interface).
*   **Controlador:** Integrado en el vidrio (COG). Compatible con drivers genéricos de Pervasive Displays.
*   **Pines de Control:**
    *   **CS (Chip Select):** P0_1
    *   **DC (Data/Command):** P1_2 (Bajo = Comando, Alto = Dato)
    *   **BUSY:** P1_3 (Entrada, Alto = Ocupado)
    *   **RESET:** P2_0
    *   **PWR (Power Enable):** P0_0 (Controla un transistor para dar energía a la pantalla)

## Funcionamiento

### 1. Inicialización (`epd_init`)
Prepara la pantalla para recibir datos.
1.  Configura pines SPI y GPIO.
2.  Enciende la alimentación (`EPD_PWR_ON`).
3.  Genera un pulso de Reset por hardware.
4.  Envía comandos de "Soft Reset".
5.  Configura parámetros como temperatura (importante para la forma de onda de la tinta) y PSR.

### 2. Actualización de Imagen (`epd_globalUpdate`)
Las pantallas Spectra tienen dos canales de color:
*   **Canal Negro/Blanco:** Define los píxeles negros y blancos.
*   **Canal Rojo:** Define los píxeles rojos.

La función recibe dos punteros a buffers de imagen:
```c
void epd_globalUpdate(const uint8_t * data1s, const uint8_t * data2s)
```
1.  Envía el comando `0x10` seguido de los datos del canal B/N.
2.  Envía el comando `0x13` seguido de los datos del canal Rojo.
3.  Llama a `epd_flushDisplay()` para iniciar el refresco físico.

### 3. Refresco (`epd_flushDisplay`)
1.  Enciende el conversor DC/DC interno de la pantalla.
2.  Envía el comando "Display Refresh" (`0x12` o `0x11`).
3.  Espera a que el pin **BUSY** se libere (indica que la pantalla terminó de actualizarse).
4.  Apaga el conversor DC/DC.

### 4. Apagado (`epd_powerOff`)
Es crítico apagar correctamente la pantalla para evitar daños y consumo de energía ("Ghosting" o quemado).
1.  Apaga el DC/DC.
2.  Pone el bus SPI y pines de control en estado seguro (bajo).
3.  Corta la alimentación principal (`EPD_PWR_OFF`).

## Formato de Datos
La imagen se envía como un array de bytes. Cada bit representa un píxel.
*   **Tamaño del Buffer:** Depende de la resolución (ej. 2.66" = 152x296 píxeles).
*   `BUFFER_SIZE = (HRES / 8) * VRES`
