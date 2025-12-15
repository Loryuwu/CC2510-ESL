#ifndef I2C_H
#define I2C_H

#include "../hal/hal.h"
#include <stdint.h>
#include <stdbool.h>

// I2C Pin Definitions
// SDA = P0_4
// SCL = P0_6

// Initialize I2C Pins
void i2c_init(void);

// Start Condition
void i2c_start(void);

// Stop Condition
void i2c_stop(void);

// Write Byte
// Returns true if ACK received, false if NACK
bool i2c_write_byte(uint8_t byte);

// Read Byte
// ack: true to send ACK, false to send NACK (last byte)
uint8_t i2c_read_byte(bool ack);

#endif // I2C_H
