# 07. Módulo COBS UART

Este módulo (`src/cobs/cobs.c`) implementa una capa de transporte robusta sobre la conexión serial UART.

## ¿Qué es COBS?
**Consistent Overhead Byte Stuffing (COBS)** es un algoritmo que permite codificar paquetes de datos de longitud variable de manera que el byte `0x00` nunca aparezca dentro del cuerpo del mensaje. Esto permite usar el byte `0x00` como un delimitador inequívoco de fin de paquete.

### Ventajas
*   **Sincronización:** Si el receptor pierde bytes o se conecta a mitad de una transmisión, puede resincronizarse fácilmente esperando el siguiente byte `0x00`.
*   **Eficiencia:** Solo añade una sobrecarga mínima (aprox. 1 byte cada 254 bytes de datos).

## Implementación

### Estructura de Estado (`CobsState`)
Mantiene el estado de la decodificación en curso:
```c
typedef struct {
  uint8_t packet[MAX_PACKET_SIZE]; // Buffer para el paquete decodificado
  uint8_t packet_size;             // Tamaño actual del paquete
  uint8_t packet_index;            // Índice de escritura
  uint8_t block, code;             // Variables internas del algoritmo
} CobsState;
```

### Funciones Principales

#### `cobs_init(CobsState *state)`
Reinicia la máquina de estados para comenzar a recibir un nuevo paquete.

#### `cobs_handle(CobsState *state)`
Esta función debe llamarse periódicamente en el bucle principal.
1.  Intenta leer un byte de la UART (`uart_read_byte`).
2.  Si hay dato, procesa el byte según el algoritmo COBS.
3.  Si el byte recibido es `0x00` y el paquete es válido, retorna `true` indicando que hay un paquete completo en `state->packet`.

#### `cobs_send(const uint8_t *data, uint8_t size)`
Codifica y envía un buffer de datos.
1.  Recorre los datos calculando los bloques y códigos COBS.
2.  Envía los bytes codificados por UART.
3.  Termina enviando un byte `0x00` como delimitador.

## Uso en el Proyecto
El sistema utiliza COBS para:
1.  **Depuración:** Enviar mensajes de texto (`cobs_send_str`) que pueden ser visualizados en el PC.
2.  **Comandos:** Recibir instrucciones complejas desde el PC (ej. "Actualizar imagen", "Configurar RF") sin riesgo de desincronización.

### Script de Prueba (`cobs_test.py`)
Se incluye un script en Python que utiliza la librería `cobs` y `pyserial` para comunicarse con el firmware desde un PC, demostrando el envío y recepción de paquetes codificados.
