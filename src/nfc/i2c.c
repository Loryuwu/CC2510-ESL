#include "i2c.h"
#include "../hal/time.h"

// Define SDA and SCL Pins
#define SDA_PIN P0_4
#define SCL_PIN P0_6

// Macros for Direction Control
// P0DIR: 1 = Output, 0 = Input
#define SDA_OUT()   P0DIR |= (1<<4)
#define SDA_IN()    P0DIR &= ~(1<<4)

#define SCL_OUT()   P0DIR |= (1<<6)

// Delay for bit-banging (adjust for speed)
#define I2C_DELAY() delay_us(5) 

// Helper for microsecond delay without timer dependency if needed
// Or just use empty loops. 
void delay_us(uint8_t us) {
    while(us--) {
        __asm nop __endasm;
        __asm nop __endasm;
        __asm nop __endasm;
        __asm nop __endasm;
    }
}

void i2c_init(void) {
    // Configure Pins
    SCL_OUT();
    SDA_OUT();
    
    // Default High (Idle)
    SDA_PIN = 1;
    SCL_PIN = 1;
}

void i2c_start(void) {
    SDA_OUT();
    SDA_PIN = 1;
    SCL_PIN = 1;
    I2C_DELAY();
    SDA_PIN = 0;
    I2C_DELAY();
    SCL_PIN = 0;
}

void i2c_stop(void) {
    SDA_OUT();
    SDA_PIN = 0;
    SCL_PIN = 1;
    I2C_DELAY();
    SDA_PIN = 1;
    I2C_DELAY();
}

bool i2c_write_byte(uint8_t byte) {
    SDA_OUT();
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & 0x80) {
            SDA_PIN = 1;
        } else {
            SDA_PIN = 0;
        }
        byte <<= 1;
        
        I2C_DELAY();
        SCL_PIN = 1;
        I2C_DELAY();
        SCL_PIN = 0;
        I2C_DELAY();
    }
    
    // ACK / NACK
    SDA_IN(); // Release SDA
    I2C_DELAY();
    SCL_PIN = 1;
    I2C_DELAY();
    
    bool ack = (SDA_PIN == 0); // Low means ACK
    
    SCL_PIN = 0;
    SDA_OUT(); // Reclaim SDA
     SDA_PIN = 0; // Drive low safely
    
    return ack;
}

uint8_t i2c_read_byte(bool ack) {
    uint8_t byte = 0;
    
    SDA_IN();
    for (uint8_t i = 0; i < 8; i++) {
        byte <<= 1;
        I2C_DELAY();
        SCL_PIN = 1;
        I2C_DELAY();
        if (SDA_PIN) {
            byte |= 1;
        }
        SCL_PIN = 0;
    }
    
    // Send ACK/NACK
    SDA_OUT();
    if (ack) {
        SDA_PIN = 0;
    } else {
        SDA_PIN = 1;
    }
    
    I2C_DELAY();
    SCL_PIN = 1;
    I2C_DELAY();
    SCL_PIN = 0;
    
    return byte;
}
