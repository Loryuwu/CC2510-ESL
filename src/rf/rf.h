#ifndef _RF_H_
#define _RF_H_

#include "../hal/hal.h"
#include <stdint.h>

// Configuración de RF
#define RF_CHANNEL         0x14     // Canal RF (0-255) 2.4068 GHz aprox
#define RF_ADDR            0x01     // Dirección del dispositivo
#define RF_POWER           0xFE     // Potencia de transmisión (-30 to 10 dBm)
#define RF_PACKET_LENGTH   32       // Longitud máxima de paquete

// Estados de RF
typedef enum {
    RF_STATE_IDLE = 0,
    RF_STATE_RX,
    RF_STATE_TX
} RFState;

// Estructura para paquete RF
typedef struct {
    uint8_t length;
    uint8_t data[RF_PACKET_LENGTH];
} RFPacket;

// Inicialización del módulo RF
void rf_init(void);

void rf_send(uint8_t *data, uint8_t len);


#if 0
// Inicialización del módulo RF
void rf_init(void);

// Configurar canal RF (0-255)
void rf_set_channel(uint8_t channel);

// Configurar potencia de transmisión
void rf_set_power(uint8_t power);

// Cambiar a modo recepción
void rf_start_rx(void);

// Enviar un paquete
// Returns: true si se envió correctamente
bool rf_send_packet(const uint8_t *data, uint8_t length);

// Verificar si hay un paquete recibido
// Returns: true si hay un paquete disponible
bool rf_packet_available(void);

// Leer el último paquete recibido
// Returns: true si se leyó un paquete
bool rf_read_packet(RFPacket *packet);

// Obtener RSSI de la última recepción
int8_t rf_get_rssi(void);

// Obtener LQI del último paquete
uint8_t rf_get_lqi(void);
#endif // 0
#endif // _RF_H_