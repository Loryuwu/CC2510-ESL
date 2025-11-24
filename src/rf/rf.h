#ifndef RF_H
#define RF_H

#include <stdint.h>

// RF Settings
#define RF_CHANNEL              0x00
#define RF_PACKET_LENGTH        32

// Function Prototypes
void rf_init(void);
uint8_t rf_send_packet(uint8_t* data, uint8_t length);
uint8_t rf_receive_packet(uint8_t* buffer, uint8_t max_length);

#endif // RF_H
