# Módulo SPI Flash para CC2510

Este módulo proporciona una interfaz para comunicarse con memorias Flash SPI (como la Winbond W25X10CL) utilizando el microcontrolador CC2510. Utiliza el periférico USART1 en modo SPI (Ubicación Alternativa 2).

## Configuración de Pines

El módulo asume la siguiente conexión de hardware:

| Señal Flash | Pin CC2510 | Función |
|-------------|------------|---------|
| **CS**      | P1_4       | Chip Select (GPIO Salida) |
| **CLK**     | P1_5       | Reloj SPI (USART1 Alt 2) |
| **MOSI**    | P1_6       | Master Out Slave In (USART1 Alt 2) |
| **MISO**    | P1_7       | Master In Slave Out (USART1 Alt 2) |

## Descripción de Funciones

### `void spi_flash_init(void)`
Inicializa el periférico USART1 en modo SPI Master y configura los pines necesarios.
- Configura P1_5, P1_6 y P1_7 como periféricos.
- Configura P1_4 como salida (CS) y lo pone en estado alto (deseleccionado).
- Establece la velocidad de baudios y el modo SPI (CPOL=0, CPHA=0).

### `uint16_t spi_flash_read_id(void)`
Lee el identificador del fabricante y del dispositivo.
- **Retorna**: Un valor de 16 bits donde el byte alto es el ID del fabricante y el bajo el ID del dispositivo.

### `void spi_flash_read(uint32_t addr, uint8_t *buf, uint16_t len)`
Lee datos desde la memoria flash.
- `addr`: Dirección de memoria de 24 bits desde donde empezar a leer.
- `buf`: Puntero al buffer donde se guardarán los datos leídos.
- `len`: Cantidad de bytes a leer.
- **Nota**: Esta función espera a que la flash no esté ocupada antes de comenzar.

### `void spi_flash_write(uint32_t addr, const uint8_t *buf, uint16_t len)`
Escribe datos en la memoria flash. Realiza automáticamente la habilitación de escritura (`WRITE ENABLE`) y la programación de página (`PAGE PROGRAM`).
- `addr`: Dirección de memoria de 24 bits donde empezar a escribir.
- `buf`: Puntero a los datos a escribir.
- `len`: Cantidad de bytes a escribir.
- **Nota**: La escritura se realiza por páginas. Asegúrese de no cruzar límites de página si la memoria lo requiere, o maneje la paginación en capas superiores.

### `void spi_flash_sector_erase(uint32_t addr)`
Borra un sector de 4KB.
- `addr`: Dirección dentro del sector que se desea borrar.

### `void spi_flash_chip_erase(void)`
Borra todo el contenido del chip.
- **Advertencia**: Esta operación puede tardar varios segundos.

### `void spi_flash_power_down(void)`
Pone la memoria flash en modo de bajo consumo.

### `void spi_flash_release_power_down(void)`
Saca la memoria flash del modo de bajo consumo.

## Ejemplos de Uso

### Inicialización y Verificación

```c
#include "flash/flash.h"

void main(void) {
    // Inicializar el sistema y el módulo flash
    spi_flash_init();

    // Leer el ID para verificar comunicación
    uint16_t flash_id = spi_flash_read_id();
    
    // Winbond W25X10CL debería retornar algo como 0xEF10
    if (flash_id == 0xEF10) {
        // Comunicación exitosa
    }
}
```

### Lectura y Escritura de Datos

```c
uint8_t datos_a_escribir[] = {0x10, 0x20, 0x30, 0x40};
uint8_t buffer_lectura[4];
uint32_t direccion = 0x000000;

// 1. Borrar el sector antes de escribir (necesario si no está vacío)
spi_flash_sector_erase(direccion);

// 2. Escribir los datos
spi_flash_write(direccion, datos_a_escribir, 4);

// 3. Leer los datos para verificar
spi_flash_read(direccion, buffer_lectura, 4);

// buffer_lectura ahora debería contener {0x10, 0x20, 0x30, 0x40}
```

### Gestión de Energía

```c
// Al terminar de usar la flash, ponerla en bajo consumo
spi_flash_power_down();

// ... tiempo después ...

// Despertar la flash antes de usarla nuevamente
spi_flash_release_power_down();
// Es recomendable esperar unos microsegundos según el datasheet antes de enviar comandos
```
