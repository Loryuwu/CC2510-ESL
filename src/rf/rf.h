/*
    RF Control Module for CC2510
    Based on OpenSky project drivers
    Standalone module
*/

#ifndef RF_H_
#define RF_H_

#include <stdint.h>

// Initialize the RF module
void rf_init(void);

// Send a data packet
// data: pointer to the data buffer
// len: length of the data to send
void rf_send_packet(uint8_t *data, uint8_t len);

// Receive a packet
// buffer: pointer to the buffer to store received data
// max_len: maximum length of the buffer
// Returns: number of bytes received, or 0 if no packet
uint8_t rf_receive_packet(uint8_t *buffer, uint8_t max_len);

// Set the RF channel
// channel: channel number
void rf_set_channel(uint8_t channel);

#endif /* RF_H_ */
