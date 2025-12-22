# Comandos del Controlador de Pantalla (Serie UC81xx)

Este documento detalla todos los comandos admitidos por el controlador de la pantalla de tinta electrónica (Serie UC81xx, como el UC8151/UC8176). Estos comandos se envían a través del bus SPI con el pin **DC** (Data/Command) en bajo (`0`) para el índice del comando y en alto (`1`) para sus parámetros.

## Tabla de Referencia Rápida

| Hex  | Comando (Nombre) | Parámetros | Descripción |
| :--- | :--- | :---: | :--- |
| `0x00` | **PSR** (Panel Setting) | 2 | Configura resolución, modo de color, etc. |
| `0x01` | **PWR** (Power Setting) | 5 | Configura voltajes de VGH, VGL, VDH, VDL. |
| `0x02` | **POF** (Power OFF) | 0 | Apaga el sistema DC/DC y detiene la salida PWM. |
| `0x03` | **PFS** (Power OFF Sequence) | 1 | Define el tiempo de caída de voltajes. |
| `0x04` | **PON** (Power ON) | 0 | Enciende el sistema DC/DC y habilita salidas de voltaje. |
| `0x05` | **PMES** (Power ON Measure) | 0 | Mide voltajes internos (autodiagnóstico). |
| `0x06` | **BTST** (Booster Soft Start) | 3 | Configura el arranque suave de los Boosters. |
| `0x07` | **DSLP** (Deep Sleep) | 1 | Entra en modo de consumo ultra bajo. |
| `0x10` | **DTM1** (Data Start Transmission 1) | Buffer | Envía el primer frame (Negro/Blanco). |
| `0x11` | **DRF** (Display Refresh) | 0 | Inicia el refresco físico de la pantalla. |
| `0x12` | **DSP** (Display Start) | 0 | Similar a 0x11, inicia la visualización. |
| `0x13` | **DTM2** (Data Start Transmission 2) | Buffer | Envía el segundo frame (Rojo/Color). |
| `0x20` | **LUTC** (VCOM LUT) | Variable | Tabla de consulta para voltajes VCOM. |
| `0x21` | **LUTW** (White LUT) | Variable | Tabla de consulta para transiciones a blanco. |
| `0x22` | **LUTB** (Black LUT) | Variable | Tabla de consulta para transiciones a negro. |
| `0x23` | **LUTR** (Red LUT) | Variable | Tabla de consulta para transiciones a rojo. |
| `0x24` | **LUTV** (VBD LUT) | Variable | Tabla de consulta para el borde (Border). |
| `0x30` | **OSC** (PLL Control) | 1 | Configura la frecuencia del oscilador interno. |
| `0x40` | **TSC** (Temperature Sensor) | 1 | Lee o selecciona el sensor de temperatura. |
| `0x50` | **CDI** (VCOM and Data Interval) | 1 | Configura el intervalo de VCOM y datos de borde. |
| `0x60` | **TCON** (TCON Setting) | 1 | Configura los retardos de S2G y G2S. |
| `0x61` | **TRES** (Resolution Setting) | 3 | Define la resolución de la pantalla. |
| `0x82` | **VDCS** (VCOM DC Setting) | 1 | Configura el voltaje DC de VCOM. |
| `0xE0` | **CASCADE** (Cascade Setting) | 1 | Configura modo cascada para pantallas múltiples. |
| `0xE5` | **TSSET** (Temperature Set) | 1 | Establece manualmente la temperatura de trabajo. |

---

## Detalle de Comandos Principales

### `0x00` PSR (Panel Setting)
Configura aspectos fundamentales del panel.
- **Byte 1**: `[RES1 RES0] [REG] [KW/R] [UD] [SHL] [SHD_N] [RST_N]`
    - `RES[1:0]`: Resolución (ej. 00=160x296, 01=128x296, 11=TRES defines).
    - `REG`: 0=Externa, 1=Interna (LDO).
    - `KW/R`: 0=B/W/R (3 colores), 1=B/W (2 colores).
    - `UD`: 0=Scan Arriba a Abajo, 1=Abajo a Arriba.
    - `SHL`: 0=Scan Izquierda a Derecha, 1=Derecha a Izquierda.
    - `SHD_N`: 0=Booster OFF, 1=Booster ON.
    - `RST_N`: 0=Soft Reset, 1=Normal.

### `0x10` y `0x13` Transmisión de Imagen
- **`0x10` (DTM1)**: Recibe una cantidad de bytes igual a `(Ancho / 8) * Alto`. Cada bit `1` representa Blanco y `0` representa Negro.
- **`0x13` (DTM2)**: En pantallas de 3 colores, este buffer define el componente Rojo.

### `0x50` CDI (VCOM and Data Interval Setting)
Crucial para el aspecto del borde de la pantalla y el refresco.
- **Byte 1**: `[VBD1 VBD0] [DDX1 DDX0] [CDI3 CDI2 CDI1 CDI0]`
    - `VBD`: Color del borde (B/W/R).
    - `DDX`: Control de datos (si se invierten o no).
    - `CDI`: Tiempo de intervalo entre frames.

---

## Cómo se usan en este proyecto

En el archivo [epd.c](file:///c:/Users/pablo/Desktop/Lory/cosas%20varias/CC25xx/imagotag-hack-master/firmware/src/display/epd.c), estos comandos se implementan mediante funciones de conveniencia:

1.  **Inicialización**: Se usa `epd_init()` que envía `0x00`, `0xE0`, `0xE5` y otros para preparar el panel.
2.  **Limpieza**: `epd_clearDisplay()` usa `0x10` y `0x13` enviando ceros para borrar la memoria.
3.  **Actualización**: `epd_flushDisplay()` llama a `0x04` (Power ON), envía `0x11` (Refresh), espera el pin `BUSY` y finalmente llama a `0x02` (Power OFF).
4.  **Streaming**: Las funciones `epd_stream_start(index)` permiten iniciar el envío de una imagen (usando `0x10` o `0x13`) y mandar los datos por piezas.

## Parámetros de Seguridad y Tiempos
- Siempre se debe verificar el pin **BUSY** (`0` es ocupado) después de comandos como `0x04` (Power ON), `0x11`/`0x12` (Refresh) y `0x00` (Reset).
- Los comandos de voltaje (`0x01`, `0x82`) pueden dañar la pantalla si se configuran fuera de los rangos del fabricante.

---

## Actualización Parcial (Partial Refresh)

Para actualizar solo un sector de la pantalla (por ejemplo, un reloj o un contador) sin que toda la pantalla parpadee, se utiliza el modo **Partial Window**.

### Pasos conceptuales:
1.  **Activar Modo Parcial**: Enviar el comando `0x91` (Partial In).
2.  **Definir Ventana**: Enviar el comando `0x90` (Partial Window) seguido de las coordenadas:
    - `HRST` (Horizontal Start), `HRED` (Horizontal End)
    - `VRST` (Vertical Start), `VRED` (Vertical End)
    - `PT_SCAN` (Modo de escaneo)
3.  **Enviar Datos**: Enviar solo los bytes correspondientes a esa área usando `0x10` (o el comando de transferencia de datos).
4.  **Refrescar**: Enviar `0x11` (Display Refresh).
5.  **Salir del Modo Parcial**: Enviar `0x92` (Partial Out).

### El problema del "Flicker"
Por defecto, el comando `0x11` o `0x12` realiza un ciclo completo de limpieza (blanco-negro-blanco) para evitar imágenes fantasma. Para una actualización "limpia" (sin parpadeo), el controlador necesita una **LUT (Look-Up Table)** específica cargada en el registro de la pantalla. 

> [!NOTE]
> En muchas pantallas de 3 colores (B/W/R), el refresco parcial no es posible o solo funciona en blanco y negro, ya que el pigmento rojo requiere ciclos de voltaje muy largos y agresivos que afectan a toda la superficie del panel.

### Ejemplo de implementación (C):
```c
void epd_setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint8_t params[9];
    
    // Configura coordenadas (ejemplo para UC8176)
    params[0] = x >> 8; params[1] = x & 0xF8; // HRST (X debe ser múltiplo de 8)
    params[2] = (x + w - 1) >> 8; params[3] = (x + w - 1) | 0x07; // HRED
    params[4] = y >> 8; params[5] = y & 0xFF; // VRST
    params[6] = (y + h - 1) >> 8; params[7] = (y + h - 1) & 0xFF; // VRED
    params[8] = 0x01; // PT_SCAN
    
    epd_sendIndexData(0x90, params, 9);
}
```
