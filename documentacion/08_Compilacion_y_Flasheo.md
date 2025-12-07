# 08. Compilación y Flasheo

Guía para construir el firmware y programarlo en el chip CC2510.

## Requisitos Previos

### Software
1.  **SDCC (Small Device C Compiler):** Compilador C optimizado para microcontroladores de 8 bits (8051).
2.  **Make:** Herramienta de automatización de construcción (GNU Make).
3.  **SmartRF Flash Programmer (v1):** Software de Texas Instruments para programar el chip (Windows).

### Hardware
1.  **CC-Debugger:** Programador/Depurador oficial de TI para la familia CCxxxx.
2.  **Cables de Conexión:** Se deben soldar cables a los pads de programación de la etiqueta ESL.
    *   **GND**
    *   **VCC** (3.3V)
    *   **DC** (Debug Clock - P2_2)
    *   **DD** (Debug Data - P2_1)
    *   **RST** (Reset - P2_0)

## Compilación

El proyecto incluye un `Makefile` que simplifica el proceso.

### Comandos
Abrir una terminal en el directorio `firmware/` y ejecutar:

1.  **Compilar todo:**
    ```bash
    make
    ```
    Esto generará el archivo `firmware.hex` si no hay errores.

2.  **Limpiar archivos temporales:**
    ```bash
    make clean
    ```
    Elimina archivos `.rel`, `.asm`, `.lst`, `.sym`, etc., generados durante la compilación.

## Flasheo

1.  Conectar el **CC-Debugger** al PC y a la etiqueta ESL (asegurar que la luz del debugger esté en verde).
2.  Abrir **SmartRF Flash Programmer**.
3.  Seleccionar "System-on-Chip" en la pestaña superior.
4.  Debería aparecer el dispositivo "CC2510" en la lista.
5.  En "Flash image", buscar y seleccionar el archivo `firmware.hex` generado.
6.  En "Actions", marcar "Erase, program and verify".
7.  Hacer clic en "Perform actions".

Si todo es correcto, la barra de progreso se completará y el firmware comenzará a ejecutarse en la etiqueta.
