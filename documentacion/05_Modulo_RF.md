# 05. Módulo RF (Radiofrecuencia)

El CC2510 integra un transceptor de RF compatible con 2.4 GHz (similar al CC2500). El módulo `src/rf/rf.c` gestiona la configuración y transmisión de datos.

## Características Configuredas
*   **Frecuencia:** 2433 MHz (Configurable via registros `FREQ2`, `FREQ1`, `FREQ0`).
*   **Modulación:** MSK (Minimum Shift Keying).
*   **Velocidad de Datos:** 250 kBaud.
*   **Potencia:** 0 dBm (Configurable en `PA_TABLE0`).
*   **Paquetes:** Longitud variable, con CRC activado.

## Arquitectura del Driver

### 1. DMA (Direct Memory Access)
El driver utiliza el controlador DMA del CC2510 para mover datos entre la memoria RAM y el buffer del radio (RFD) automáticamente. Esto libera a la CPU y permite transferencias rápidas.
*   **Canal DMA 0:** Configurado dinámicamente para TX o RX.
*   **TX:** RAM -> RFD.
*   **RX:** RFD -> RAM.

### 2. Máquina de Estados (`RFST`)
El radio se controla enviando comandos estroboscópicos (Strobe Commands) al registro `RFST`:
*   `RFST_SIDLE` (0x04): Pone el radio en modo inactivo (Idle).
*   `RFST_STX` (0x03): Inicia la transmisión.
*   `RFST_SRX` (0x02): Inicia la recepción.

### 3. API Principal

#### `rf_init()`
Configura todos los registros del radio (Modem, Frecuencia, AGC, etc.) con valores precalculados (generalmente obtenidos con SmartRF Studio). Configura el DMA y deja el radio en modo RX por defecto.

#### `rf_send_packet(uint8_t *data, uint8_t len)`
1.  Prepara un buffer interno con el formato `[Longitud, Datos...]`.
2.  Pone el radio en IDLE.
3.  Configura el DMA para transferir desde el buffer al registro `X_RFD`.
4.  Activa el DMA.
5.  Envía el comando `STX`. El radio solicitará datos al DMA a medida que transmite.
6.  Espera a que termine la transmisión (Interrupción `RFIF_IRQ_DONE`).
7.  Vuelve a modo RX.

#### `rf_receive_packet(uint8_t *buffer, uint8_t max_len)`
Verifica si se ha recibido un paquete completo.
1.  Comprueba el flag de interrupción `RFIF_IRQ_DONE` o el estado del DMA.
2.  Si hay datos, lee la longitud del primer byte.
3.  Copia los datos del buffer interno del driver al buffer del usuario.
4.  Re-arma el modo RX para el siguiente paquete.

## Consideraciones
*   La calibración del sintetizador de frecuencia (`FSCAL`) se realiza automáticamente al pasar de IDLE a TX/RX (configurado en `MCSM0`).
*   Es crucial que el oscilador de cristal (XOSC) esté estable antes de usar el radio.
