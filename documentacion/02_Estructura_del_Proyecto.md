# 02. Estructura del Proyecto

Este documento detalla la organización de los archivos y directorios del proyecto, explicando el propósito de cada componente.

## Árbol de Directorios
```
firmware/
├── Makefile                # Script de compilación automatizada
├── README.md               # Documentación rápida original
├── cobs_test.py            # Script Python para pruebas de comunicación serial
├── firmware.hex            # Binario compilado (salida)
├── src/                    # Código fuente del firmware
│   ├── main.c              # Punto de entrada y lógica principal
│   ├── hal/                # Capa de Abstracción de Hardware (HAL)
│   │   ├── clock.c/h       # Configuración del reloj del sistema
│   │   ├── hal.h           # Macros y definiciones generales
│   │   ├── isr.h           # Definiciones de interrupciones
│   │   ├── led.c/h         # Control de LEDs
│   │   ├── time.c/h        # Funciones de tiempo (delay, millis)
│   │   └── uart.c/h        # Driver de comunicación serial
│   ├── display/            # Drivers de pantalla
│   │   └── epd.c/h         # Controlador para pantalla E-Paper
│   ├── flash/              # Drivers de memoria
│   │   └── flash.c/h       # Controlador para Flash SPI externa
│   ├── rf/                 # Drivers de Radio
│   │   └── rf.c/h          # Controlador del transceptor RF
│   ├── cobs/               # Protocolos de comunicación
│   │   └── cobs.c/h        # Implementación de codificación COBS
│   └── image_data/         # Datos estáticos
│       └── image.h         # Arrays de bytes con imágenes de prueba
├── doc/                    # Documentación técnica y datasheets
└── img/                    # Imágenes para documentación
```

## Descripción de Módulos

### 1. `src/main.c`
Es el archivo principal del firmware. Contiene la función `main()` que:
1.  Inicializa el reloj y los periféricos (HAL).
2.  Ejecuta secuencias de prueba (definidas por bloques `#if`).
3.  Contiene el bucle infinito (`while(1)`) que gestiona la lógica de la aplicación (ej. esperar paquetes RF, actualizar pantalla).

### 2. `src/hal/` (Hardware Abstraction Layer)
Contiene los drivers de bajo nivel que interactúan directamente con los registros del CC2510.
*   **clock**: Configura los osciladores. El CC2510 arranca con un oscilador RC interno (impreciso) y debe cambiar al cristal externo para usar RF y UART correctamente.
*   **led**: Abstrae el control de los pines conectados a los LEDs.
*   **uart**: Maneja la configuración del puerto serial (baudrate, pines) y el envío/recepción de bytes.
*   **time**: Proporciona funciones de retardo bloqueante (`delay_ms`) y conteo de tiempo (`millis`) usando Timers.

### 3. `src/display/`
Contiene la lógica específica para controlar la pantalla E-Ink.
*   **epd**: Implementa el protocolo SPI por software o hardware para enviar comandos y datos al controlador de la pantalla. Maneja la secuencia de encendido, las Look-Up Tables (LUTs) si son necesarias, y el refresco del display.

### 4. `src/rf/`
Maneja el radio de 2.4 GHz.
*   **rf**: Configura los registros del radio (frecuencia, potencia, modulación), gestiona el DMA para transferencias rápidas de paquetes y controla la máquina de estados del radio (RX/TX/IDLE).

### 5. `src/flash/`
Controlador para la memoria Flash externa.
*   **flash**: Implementa comandos SPI estándar (JEDEC) para leer ID, leer datos, escribir páginas y borrar sectores de la memoria Winbond.

### 6. `src/cobs/`
Implementa el algoritmo **Consistent Overhead Byte Stuffing**.
*   **cobs**: Permite empaquetar datos para su transmisión serial, delimitando los paquetes con un byte `0x00` sin riesgo de que los datos contengan ese byte, lo que facilita la sincronización y recuperación de errores en la comunicación.

### 7. `Makefile`
Es el script utilizado por la herramienta `make` para compilar el proyecto. Define:
*   El compilador a usar (`sdcc`).
*   Los archivos fuente a compilar.
*   Las banderas de compilación (flags).
*   Las reglas para generar el archivo `.hex` final.
