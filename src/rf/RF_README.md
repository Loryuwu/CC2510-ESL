# Documentación del Módulo RF (CC2510)

## Descripción General

Este módulo implementa las funciones de comunicación por radio para el microcontrolador CC2510. Proporciona una capa de abstracción para configurar y utilizar el transceptor RF integrado, permitiendo la comunicación inalámbrica en la banda de 2.4 GHz.

## Características Principales

- Frecuencia: 2.4 GHz ISM band
- Tasa de datos: 250 kbps (configurable)
- Potencia TX: hasta +1 dBm
- Sensibilidad RX: -95 dBm típico
- CRC automático de 16 bits
- Buffer FIFO de 64 bytes para TX y RX
- Medición de RSSI y LQI

## Arquitectura del Módulo

### Archivos
- `rf.h`: Interfaz pública del módulo
- `rf.c`: Implementación de las funciones

### Estados de Radio
```c
typedef enum {
    RF_STATE_IDLE,  // Radio en reposo
    RF_STATE_RX,    // Recibiendo
    RF_STATE_TX     // Transmitiendo
} rf_state_t;
```

## API Pública

### Inicialización
```c
void rf_init(void)
```
- Configura registros RF base
- Establece frecuencia base (2.4 GHz)
- Configura velocidad de datos (250 kbps)
- Habilita CRC automático
- Configura interrupciones RF
- Estado inicial: IDLE

### Configuración
```c
void rf_set_channel(uint8_t channel)
```
- Configura el canal RF (0-255)
- Fórmula frecuencia: 2400 MHz + (channel * 0.4) MHz
- Ejemplo: canal 10 = 2404 MHz

```c
void rf_set_power(uint8_t power)
```
- Configura potencia TX (0-255)
- Mapeo interno a niveles de potencia del CC2510:
  - 0-63: -30 dBm
  - 64-127: -12 dBm
  - 128-191: -6 dBm
  - 192-255: 0 dBm

### Transmisión
```c
bool rf_send_packet(const uint8_t *data, uint8_t length)
```
- Envía un paquete RF
- Parámetros:
  - data: buffer con datos a enviar
  - length: longitud del paquete (máx 64 bytes)
- Retorna:
  - true: transmisión iniciada correctamente
  - false: error o radio ocupada

### Recepción
```c
uint8_t rf_receive_packet(uint8_t *buffer)
```
- Lee paquete recibido si disponible
- Parámetros:
  - buffer: donde almacenar datos recibidos
- Retorna:
  - 0: no hay paquete disponible
  - >0: longitud del paquete recibido

### Diagnóstico
```c
int8_t rf_get_rssi(void)
```
- Obtiene RSSI del último paquete
- Retorna: valor en dBm (-128 a +127)
- Fórmula: RSSI_dBm = RSSI_reg/2 - RSSI_OFFSET

```c
uint8_t rf_get_lqi(void)
```
- Obtiene Link Quality Indicator
- Retorna: 0 (peor) a 255 (mejor)

```c
rf_state_t rf_get_state(void)
```
- Obtiene estado actual del radio
- Útil para diagnóstico/depuración

## Manejo de Interrupciones

El módulo usa la interrupción RF (RF_VECTOR) para:
1. TX completo: limpia flags, vuelve a IDLE
2. RX completo: lee paquete del FIFO
3. Errores: overflow, timeout, CRC

## Ejemplo de Uso

```c
// Inicialización
rf_init();
rf_set_channel(10);  // 2404 MHz
rf_set_power(200);   // ~0 dBm

// Transmisión
uint8_t data[] = {1, 2, 3, 4};
if (rf_send_packet(data, sizeof(data))) {
    // TX iniciada correctamente
}

// Recepción
uint8_t buffer[64];
uint8_t len = rf_receive_packet(buffer);
if (len > 0) {
    // Paquete recibido
    int8_t rssi = rf_get_rssi();
    uint8_t lqi = rf_get_lqi();
}
```

## Configuración Avanzada

### Registros Clave
- SYNC1/SYNC0: Palabra de sincronización (0xD391 default)
- ADDR: Dirección del dispositivo
- CHANNR: Número de canal
- FSCTRL1: Frecuencia intermedia
- MDMCFG4-0: Configuración del modem
- PKTLEN: Longitud máxima de paquete
- PKTCTRL0/1: Control de paquetes
- PA_TABLE0: Tabla de potencia TX

### Optimización
1. Ajuste de filtro IF:
   - MDMCFG4.CHANBW_E/M para ancho de banda
   - Mayor ancho = mejor sensibilidad pero más ruido

2. Formato de paquete:
   - Variable/fijo via PKTCTRL0
   - Con/sin CRC
   - Opciones de whitening

3. Potencia TX:
   - PA_TABLE0 para ajuste fino
   - Balance consumo vs alcance

## Troubleshooting

### Problemas Comunes

1. No recepción:
   - Verificar canal coincide entre TX/RX
   - Comprobar RSSI para interferencias
   - Validar formato de paquete

2. CRC errors:
   - Verificar configuración coincide TX/RX
   - Revisar interferencias (RSSI)
   - Reducir tasa de datos

3. Alcance pobre:
   - Verificar potencia TX
   - Revisar adaptación de antena
   - Considerar factores ambientales

### Herramientas de Debug

1. RSSI/LQI:
   - Monitorear calidad de enlace
   - Detectar interferencias
   - Optimizar posicionamiento

2. Estado RF:
   - rf_get_state() para verificar
   - LED de debug en ISR
   - Contador de errores

## Consideraciones de Energía

1. Modos de bajo consumo:
   - IDLE cuando no se usa
   - Sleep entre transmisiones
   - Wake-on-radio posible

2. Balance potencia vs alcance:
   - Ajustar TX power según necesidad
   - Considerar duty cycle
   - Monitorear consumo

## Referencias

- [CC2510 Datasheet](http://www.ti.com/lit/ds/symlink/cc2510.pdf)
- [CC2510 User Guide](http://www.ti.com/lit/ug/swru055/swru055.pdf)
- [RF Performance](http://www.ti.com/lit/an/swra151a/swra151a.pdf)

## Próximas Mejoras

1. Implementar wake-on-radio
2. Añadir modo beacon
3. Soporte para frequency hopping
4. Buffer circular para múltiples paquetes
5. Estadísticas detalladas (paquetes OK/error)