# Documentación del Módulo NFC (NT3H2111)

Este directorio contiene los drivers para controlar el chip **NT3H2111 (NTAG I2C Plus)** utilizando el microcontrolador CC2510.
El driver implementa un bus I2C por software (Bit-Banging) debido a la asignación de pines específica del hardware.

## Conexionado (Pinout)

| Señal | Pin CC2510 | Notas |
|-------|------------|-------|
| **SDA** | P0.4       | Compartido con UART TX |
| **SCL** | P0.6       | |
| **VCC** | P1.0       | Alimentación conmutable (Power Pin) |
| **FD**  | P1.1       | Field Detection (Interrupción de Campo) |

> **⚠️ Advertencia**: Dado que `P0.4` se comparte entre **NFC SDA** y **UART TX**, se debe tener cuidado de no utilizar la UART simultáneamente con el módulo NFC, o reconfigurar los pines adecuadamente antes de cada operación.

---

## 1. Driver I2C (`i2c.c` / `i2c.h`)

Este archivo implementa el protocolo de bajo nivel.

### `void i2c_init(void)`
**Descripción**: Configura los pines SDA y SCL como salidas y los pone en estado inactivo (HIGH).
- **Uso**: Llamar una vez al inicio o antes de comenzar transacciones si los pines fueron usados para otra cosa (ej. UART).

### `void i2c_start(void)`
**Descripción**: Genera la condición de **START** en el bus I2C (SDA baja mientras SCL está alta).
- **Propósito**: Iniciar una transacción.

### `void i2c_stop(void)`
**Descripción**: Genera la condición de **STOP** en el bus I2C (SDA sube mientras SCL está alta).
- **Propósito**: Finalizar una transacción y liberar el bus.

### `bool i2c_write_byte(uint8_t byte)`
**Descripción**: Envía un byte (8 bits) por el bus.
- **Parámetros**:
    - `byte`: El dato a enviar.
- **Retorno**: `true` si el esclavo respondió con **ACK** (se recibió el dato correctamente), `false` si hubo **NACK**.

### `uint8_t i2c_read_byte(bool ack)`
**Descripción**: Lee un byte del bus I2C.
- **Parámetros**:
    - `ack`: `true` para enviar ACK al final (indica que queremos leer más bytes), `false` para enviar NACK (indica que es el último byte).
- **Retorno**: El byte leído.

---

## 2. Driver NFC (`nfc.c` / `nfc.h`)

Este archivo implementa las funciones de alto nivel para el chip NT3H2111.

### `void nfc_init(void)`
**Descripción**: 
1. Activa la alimentación del chip NFC poniendo `P1.0` en HIGH.
2. Espera unos milisegundos para que el chip arranque.
3. Inicializa el bus I2C llamando a `i2c_init()`.
- **Uso**: Obligatorio llamar antes de cualquier operación NFC.

### `bool nfc_read_page(uint8_t page, uint8_t *buffer)`
**Descripción**: Lee una página completa (16 bytes) del tag NFC.
- **Parámetros**:
    - `page`: Dirección de la página a leer (0x00 a 0xE2 aprox).
    - `buffer`: Puntero a un array de al menos 16 bytes donde se guardarán los datos.
- **Retorno**: `true` si la lectura fue exitosa (ACK recibido), `false` en caso de error.
- **Ejemplo**:
```c
uint8_t data[16];
if (nfc_read_page(0, data)) {
    // data contiene los primeros 16 bytes de la memoria
}
```

### `bool nfc_write_page(uint8_t page, uint8_t *data)`
**Descripción**: Escribe una página completa (16 bytes) en el tag NFC.
- **Parámetros**:
    - `page`: Dirección de la página a escribir.
    - `data`: Puntero a un array de 16 bytes con los datos a escribir.
- **Retorno**: `true` si la escritura fue recibida correctamente.
- **Nota**: La función incluye un retardo (`delay_ms(10)`) al final para dar tiempo al chip a grabar los datos en su memoria EEPROM interna.

### `bool nfc_erase_all(void)`
**Descripción**: Realiza un borrado completo ("Factory Reset") del chip.
1. Escribe `0x00` en el registro de configuración (Página `0x3A`) para eliminar protecciones/passwords.
2. Escribe `0x00` en todas las páginas de memoria de usuario (Páginas 4 a `0x39`).
- **Retorno**: `true` si todo fue exitoso.
- **Tiempo de ejecución**: Aprox 2.2s.

### `bool nfc_read_session_reg(uint8_t *reg_val)`
**Descripción**: Función placeholder para leer registros de sesión (configuración). Actualmente no implementada completamente.
- **Retorno**: `false`.

---

## Dirección I2C
El driver usa la dirección I2C por defecto de la familia NTAG I2C:
- **Escritura**: `0xAA` (0x55 << 1)
- **Lectura**: `0xAB` ((0x55 << 1) | 1)

## Recomendaciones de Uso
1. **Inicialización**: Siempre llamar a `nfc_init()` primero.
2. **Watchdog**: Las operaciones de escritura tienen retardos (10ms). Si usas el Watchdog Timer (WDT), asegúrate de "alimentarlo" si vas a escribir muchas páginas seguidas en un bucle.
3. **Conflictos**: Recuerda deshabilitar la UART si la estabas usando antes de llamar a estas funciones.
