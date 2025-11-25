# ⚠️En desarrollo, aun no funcional⚠️
el proyecto esta funcionando bastante bien, la funcion de envío de datos se come las 2 primeros caracteres 
en lugar de recibir "Hola Mundo" recibe "la Mundo", aun no se por qué, pero en demás, todo correcto.

# Módulo de Radiofrecuencia (RF) para CC2510

Este módulo implementa una comunicación inalámbrica básica utilizando el transceptor de 2.4GHz integrado en el CC2510.

## Configuración
El módulo está configurado con los siguientes parámetros por defecto:
- **Frecuencia:** 2433 MHz
- **Velocidad de Datos:** 250 kBaud
- **Modulación:** MSK
- **Potencia de Salida:** 0 dBm

## Archivos
- `rf.h`: Archivo de cabecera con los prototipos de funciones y definiciones.
- `rf.c`: Implementación de las funciones de inicialización, envío y recepción.

## Cómo Usar

### 1. Inicialización
Antes de usar cualquier función de RF, debes inicializar el módulo llamando a `rf_init()`. Esto configura los registros del radio con los valores necesarios.

```c
#include "rf/rf.h"

void main(void) {
    // ... otras inicializaciones ...
    rf_init();
    // ...
}
```

### 2. Enviar Paquetes
Para enviar datos, utiliza la función `rf_send_packet`. Esta función es bloqueante y esperará hasta que el paquete se haya transmitido.

```c
uint8_t datos[] = "Hola Mundo";
rf_send_packet(datos, sizeof(datos) - 1); // Enviar sin el caracter nulo
```


### 3. Recibir Paquetes
Para recibir datos, utiliza `rf_receive_packet`. Esta función verifica si ha llegado un paquete completo.

```c
uint8_t buffer[32];
uint8_t longitud = rf_receive_packet(buffer, 32);

if (longitud > 0) {
    // Se recibió un paquete
    // 'buffer' contiene los datos
    // 'longitud' es el tamaño de los datos recibidos
}
```

**Funcionamiento interno:**
1.  Verifica si el radio está en modo RX. Si no, lo pone en modo RX (`SRX`).
2.  Verifica la bandera de interrupción `IRQ_DONE` en el registro `RFIF` (bit 4), que indica que se ha completado una operación (en este caso, recepción).
3.  Si hay un paquete:
    -   Lee la longitud del primer byte.
    -   Si la longitud es válida, lee el resto de los datos del registro `RFD`.
    -   Lee (y descarta por ahora) los bytes de estado (RSSI y LQI) que el hardware añade automáticamente.
    -   Devuelve la longitud de los datos útiles.
4.  Si la longitud es excesiva, limpia el FIFO de RX (`SFRX`) y reinicia la recepción.

## Notas
- La función de recepción está diseñada para ser llamada periódicamente (polling) dentro del bucle principal.
- Asegúrate de que `HAL_ENABLE_INTERRUPTS()` esté activo si decides usar interrupciones en el futuro, aunque esta implementación básica funciona por polling de banderas.
