# 06. Módulo Flash Externa

Este módulo (`src/flash/flash.c`) permite leer y escribir en la memoria Flash SPI externa (Winbond W25X10CL).

## Interfaz de Hardware
La memoria comparte el bus SPI con los LEDs RGB, lo que requiere una gestión cuidadosa de los pines.
*   **CS (Chip Select):** P1_4 (Salida)
*   **CLK (Clock):** P1_5 (Periférico SPI / LED Rojo)
*   **MOSI (Data In):** P1_6 (Periférico SPI / LED Azul)
*   **MISO (Data Out):** P1_7 (Periférico SPI / LED Verde)

> **Conflicto:** Al activar el periférico SPI (USART1 Alt 2), los pines P1_5, P1_6 y P1_7 dejan de funcionar como GPIO para los LEDs.

## Implementación SPI
El driver utiliza la **USART1** en modo **SPI Master Alt 2**.
*   **Configuración:** `PERCFG |= 0x02` (U1CFG Alt 2).
*   **Función de Transferencia:** `spi_transfer(uint8_t data)` escribe un byte en `U1DBUF` y espera a que se complete el intercambio.

## Comandos Soportados
El driver implementa un subconjunto de los comandos estándar JEDEC:

1.  **Lectura de ID (`spi_flash_read_id`)**
    *   Envía `0x90` (Manufacturer ID).
    *   Retorna el ID del fabricante y del dispositivo (Winbond = 0xEF). Útil para verificar conexión.

2.  **Lectura de Datos (`spi_flash_read`)**
    *   Comando `0x03`.
    *   Lee `len` bytes desde una dirección de 24 bits.

3.  **Escritura (`spi_flash_write`)**
    *   Habilita escritura (`0x06` Write Enable).
    *   Usa "Page Program" (`0x02`) para escribir datos.
    *   **Nota:** Las memorias Flash requieren que el área esté borrada (0xFF) antes de escribir.

4.  **Borrado (`spi_flash_sector_erase`, `spi_flash_chip_erase`)**
    *   Sector Erase (`0x20`): Borra un sector de 4KB.
    *   Chip Erase (`0xC7`): Borra toda la memoria.
    *   Estas operaciones toman tiempo; el driver verifica el registro de estado (bit WIP) para esperar a que terminen (`spi_flash_wait_busy`).

5.  **Power Down (`spi_flash_power_down`)**
    *   Pone la memoria en modo de ultra-bajo consumo (`0xB9`).
    *   Debe despertarse (`0xAB`) antes de volver a usarla.
