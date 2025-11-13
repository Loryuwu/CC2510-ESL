#include "rf.h"
#include "../hal/led.h"
#include "../hal/time.h"

// Macro para acceder a registros XDATA
#define XREG(addr)     (*(__xdata volatile unsigned char *)(addr))

// Variables estáticas en XDATA
static volatile __xdata RFState rf_state = RF_STATE_IDLE;
static volatile __xdata bool packet_received = false;
static __xdata RFPacket rx_packet;



#define RF_CHANNEL        0x14     // Canal 2.4068 GHz aprox
#define RF_ADDR           0x01     // Dirección del dispositivo
#define PACKET_MAX_LEN    32

volatile __bit packet_received = 0;
static uint8_t rx_buffer[PACKET_MAX_LEN];
static uint8_t rx_length = 0;

// =========================================================
// Inicialización RF básica para CC2510
// =========================================================
void rf_init(void) {
    RFST = 0x30; // SRES: reset del chip RF
    while (MARCSTATE != 0x01); // Esperar estado IDLE

    // ===== Configuración de radio básica =====
    FSCTRL1 = 0x0A;
    FSCTRL0 = 0x00;
    FREQ2   = 0x5D;
    FREQ1   = 0x93;
    FREQ0   = 0xB1;   // ~2405 MHz

    MDMCFG4 = 0x2D;
    MDMCFG3 = 0x3B;
    MDMCFG2 = 0x13;   // GFSK, sincronización
    MDMCFG1 = 0x22;
    MDMCFG0 = 0xF8;

    DEVIATN = 0x62;
    MCSM0   = 0x18;   // IDLE después de TX
    FOCCFG  = 0x1D;
    BSCFG   = 0x1C;
    AGCCTRL2= 0xC7;
    AGCCTRL1= 0x00;
    AGCCTRL0= 0xB0;
    FREND1  = 0xB6;
    FREND0  = 0x10;
    FSCAL3  = 0xEA;
    FSCAL2  = 0x0A;
    FSCAL1  = 0x00;
    FSCAL0  = 0x11;

    TEST2   = 0x88;
    TEST1   = 0x31;
    TEST0   = 0x09;

    PKTCTRL1 = 0x04;   // No address check
    PKTCTRL0 = 0x05;   // CRC habilitado, variable length
    ADDR     = RF_ADDR;
    CHANNR   = RF_CHANNEL;
    PKTLEN   = PACKET_MAX_LEN;

    // Limpia buffers y pone en modo RX
    RFST = 0x36; // SIDLE
    RFST = 0x3A; // SFRX
    RFST = 0x3B; // SFTX

    // ===== Configura interrupciones =====
    RFIF = 0;            // Limpia banderas RF
    IEN2 |= 0x01;        // Habilita interrupciones RF general
    RFIM = 0x10;         // Habilita RFIF.IRQ_DONE
    EA = 1;              // Habilita interrupciones globales

    // ===== Entra en modo recepción =====
    RFST = 0x34; // SRX
}

// =========================================================
// Enviar paquete simple
// =========================================================
void rf_send(uint8_t *data, uint8_t len) {
    if (len > RF_PACKET_LENGTH) len = RF_PACKET_LENGTH;

    RFST = 0x36; // IDLE
    RFST = 0x3B; // Limpia TX FIFO

    RFD = len;   // Longitud del paquete
    for (uint8_t i = 0; i < len; i++) {
        RFD = data[i];
    }

    RFST = 0x35; // STX

    // Espera hasta que MARCSTATE vuelva a IDLE
    while (MARCSTATE != 0x01);

    // Regresa a modo recepción
    RFST = 0x34; // SRX
}



#if 0
// Configuración de registros RF para 250 kbps
static const uint8_t rf_register_settings[] = {
    // Registros de configuración RF
    0x0E,  // FSCTRL1   Freq synthesizer control
    0x00,  // FSCTRL0   Freq synthesizer control
    0x5D, // 0x21,  // FREQ2     Freq control word, high byte
    0x93, // 0x65,  // FREQ1     Freq control word, middle byte
    0xB1, // 0x6A,  // FREQ0     Freq control word, low byte
    0xCA,  // MDMCFG4   Modem configuration
    0x83,  // MDMCFG3   Modem configuration
    0x93,  // MDMCFG2   Modem configuration
    0x22,  // MDMCFG1   Modem configuration
    0xF8,  // MDMCFG0   Modem configuration
    0x00,  // CHANNR    Channel number
    0x34,  // DEVIATN   Modem deviation setting
    0xB9,  // FREND1    Front end TX configuration
    0x56,  // FREND0    Front end RX configuration
    0xE9,  // MCSM0     Main Radio Control State Machine config
    0x30,  // FOCCFG    Freq Offset Compensation config
    0x3C,  // BSCFG     Bit Synchronization config
    0x0B,  // AGCCTRL2  AGC control
    0x6E,  // AGCCTRL1  AGC control
    0x92,  // AGCCTRL0  AGC control
    0x87,  // FSCAL3    Freq synthesizer calibration
    0x2F,  // FSCAL2    Freq synthesizer calibration
    0x1A,  // FSCAL1    Freq synthesizer calibration
    0x00,  // FSCAL0    Freq synthesizer calibration
    0x59,  // FSTEST    Freq synthesizer calibration control
    0x7F,  // TEST2     Various test settings
    0x3F,  // TEST1     Various test settings
    0x0F   // TEST0     Various test settings
};

// ISR para radio
void rf_isr(void) __interrupt (RF_VECTOR) {
    // Guardar y limpiar flags de interrupción inmediatamente
    uint8_t status = RFIF;
    RFIF = 0;
    
    if (status & 0x10) {  // TX complete
        // Forzar estado IDLE después de TX
        rf_state = RF_STATE_IDLE;
        RFST = 0x01;   // SIDLE: Asegurar que vamos a IDLE
    }
    
    if (status & 0x04) {  // RX complete
        if (rf_state == RF_STATE_RX && !packet_received) {
            HAL_CRITICAL_STATEMENT({
                rx_packet.length = RFD;
                if (rx_packet.length <= RF_PACKET_LENGTH) {
                    for (uint8_t i = 0; i < rx_packet.length; i++) {
                        rx_packet.data[i] = RFD;
                    }
                    packet_received = true;
                }
            });
        }
        // Forzar estado IDLE después de RX
        RFST = 0x01;
    }
}

void rf_init(void) {
    
    // Reset del radio
    RFST = 0x30;   // RFOFF: Fuerza al radio a idle
    delay_ms(1);
    RFST = 0x01;   // SIDLE: Ir a estado idle
    delay_ms(1);
    
    // Limpiar buffer RF
    RFST = 0x3A;   // SFTX: Flush del buffer TX
    delay_ms(1);
    RFST = 0x3B;   // SFRX: Flush del buffer RX
    delay_ms(1);

    // Configurar registros RF
    FSCTRL1 = rf_register_settings[0];
    FSCTRL0 = rf_register_settings[1];
    FREQ2 = rf_register_settings[2];
    FREQ1 = rf_register_settings[3];
    FREQ0 = rf_register_settings[4];
    MDMCFG4 = rf_register_settings[5];
    MDMCFG3 = rf_register_settings[6];
    MDMCFG2 = rf_register_settings[7];
    MDMCFG1 = rf_register_settings[8];
    MDMCFG0 = rf_register_settings[9];
    CHANNR = rf_register_settings[10];
    DEVIATN = rf_register_settings[11];
    FREND1 = rf_register_settings[12];
    FREND0 = rf_register_settings[13];
    // Configurar MCSM0 para ir a IDLE después de TX y RX
    MCSM0 = (rf_register_settings[14] & 0xF0) | 0x00;  // Estado después de RX/TX: IDLE
    FOCCFG = rf_register_settings[15];
    BSCFG = rf_register_settings[16];
    AGCCTRL2 = rf_register_settings[17];
    AGCCTRL1 = rf_register_settings[18];
    AGCCTRL0 = rf_register_settings[19];
    FSCAL3 = rf_register_settings[20];
    FSCAL2 = rf_register_settings[21];
    FSCAL1 = rf_register_settings[22];
    FSCAL0 = rf_register_settings[23];
    // Los registros de prueba están en el espacio XDATA
    XREG(0xDF2D) = rf_register_settings[24];  // FSTEST
    XREG(0xDF2C) = rf_register_settings[25];  // TEST2
    XREG(0xDF2B) = rf_register_settings[26];  // TEST1
    XREG(0xDF2A) = rf_register_settings[27];  // TEST0
    
    // Potencia TX inicial
    PA_TABLE0 = RF_POWER;
    
    // Habilitar interrupciones RF
    RFIM = 0x14;  //(1 << 4) | (1 << 3);  // Habilitar interrupciones TX_COMPLETE y RX_COMPLETE
    IEN2 |= 0x01;                // Habilitar interrupciones RF (bit 0 en IEN2)
    EA = 1;                       // Habilitar interrupciones globales
    delay_ms(10); // Pequeña espera para asegurar que los registros se configuren correctamente

    // Asegurarnos de que estamos en IDLE antes de calibrar
    RFST = 0x01;   // SIDLE: Ir a estado idle
    delay_ms(1);
    
    // Calibrar oscilador RF
    RFST = 0x01;   // Command strobe: SCAL
    while (MARCSTATE != 0x01);  // Dar tiempo para que inicie la calibración
    RFST = 0x03;   // Command strobe: SCAL
    while (MARCSTATE == 0x11);

    uint16_t timeout = 1000; // Timeout de seguridad
    while(MARCSTATE != 0x01 && timeout-- > 0) {
        delay_ms(1);
    }
    
    if (timeout == 0) {
        // Si llegamos aquí, hubo timeout en la calibración
        LED_R_ON;   // Debug: LED rojo ON indica error
        LED_G_OFF;  // Debug: LED verde OFF indica error
        LED_B_OFF;  // Debug: LED azul OFF indica error
        
        // Parpadear el LED verde según el valor de MARCSTATE
        uint8_t state = MARCSTATE;
        while(1) {
            for(uint8_t i = 0; i < state; i++) {
                LED_ON;
                delay_ms(200);
                LED_OFF;
                delay_ms(200);
            }
            delay_ms(1000);  // Pausa entre secuencias
        }
    }
    
    rf_state = RF_STATE_IDLE;
}

void rf_set_channel(uint8_t channel) {
    CHANNR = channel;
}

void rf_set_power(uint8_t power) {
    PA_TABLE0 = power;
}

void rf_start_rx(void) {
    if (rf_state != RF_STATE_RX) {
        RFST = 0x03;   // Command strobe: SCAL
        while(MARCSTATE != 0x01);
        
        RFST = 0x02;   // Command strobe: SRX
        rf_state = RF_STATE_RX;
        packet_received = false;
    }
}

bool rf_send_packet(const uint8_t *data, uint8_t length) {
    LED_B_ON;  // Indicador de entrada a la función
    LED_G_OFF;
    LED_R_OFF;
    
    if (length > RF_PACKET_LENGTH || rf_state != RF_STATE_IDLE) {
        LED_R_ON;  // Error de parámetros
        LED_B_OFF;
        return false;
    }
    
    // 1. Asegurar que estamos en IDLE
    RFST = 0x01;   // SIDLE: Ir a estado idle
    delay_ms(5);
    
    if (MARCSTATE != 0x01) {  // Si no estamos en IDLE, forzar
        RFST = 0x30;   // RFOFF: Fuerza al radio a idle
        delay_ms(5);
        RFST = 0x01;   // SIDLE: Intentar IDLE de nuevo
        delay_ms(5);
        
        if (MARCSTATE != 0x01) {
            LED_R_ON;
            LED_B_OFF;
            return false;
        }
    }
    
    // 2. Limpiar buffer TX
    RFST = 0x3A;   // SFTX: Flush del buffer TX
    delay_ms(5);
    
    // Verificar que estamos en IDLE
    uint8_t retry = 3;
    while (MARCSTATE != 0x01 && retry > 0) {
        RFST = 0x01;   // SIDLE: Ir a estado idle
        delay_ms(10);
        retry--;
    }
    
    if (MARCSTATE != 0x01) {
        // Mostrar el estado actual con LED rojo
        uint8_t state = MARCSTATE;
        for(uint8_t i = 0; i < state; i++) {
            LED_R_ON;
            delay_ms(100);
            LED_R_OFF;
            delay_ms(100);
        }
        LED_B_OFF;
        return false;
    }
    
    LED_G_ON;  // Progreso: IDLE OK
    
    // Limpiar buffer TX
    RFST = 0x3A;   // SFTX: Flush del buffer TX
    delay_ms(10);
    
    // Escribir longitud y datos
    RFD = length;
    for (uint8_t i = 0; i < length; i++) {
        RFD = data[i];
    }
    
    LED_G_OFF;  // Progreso: Datos escritos
    
    // Calibrar antes de transmitir
    RFST = 0x03;   // SCAL
    delay_ms(10);
    
    // Verificar estado antes de transmitir
    uint8_t pre_tx_state = MARCSTATE;
    if (pre_tx_state != 0x01) {  // Si no estamos en IDLE
        for(uint8_t i = 0; i < pre_tx_state; i++) {
            LED_R_ON;
            delay_ms(50);
            LED_R_OFF;
            delay_ms(50);
        }
        LED_B_OFF;
        return false;
    }
    
    // Iniciar transmisión
    rf_state = RF_STATE_TX;
    RFST = 0x04;   // STX
    
    // Esperar fin de transmisión con timeout
    uint16_t timeout = 100;  // Reducido a 100ms
    while(timeout > 0) {
        if (rf_state == RF_STATE_IDLE) {
            // Transmisión completada
            LED_G_ON;
            LED_B_OFF;
            delay_ms(10);
            LED_G_OFF;
            return true;
        }
        
        // Verificar por estados de error
        uint8_t state = MARCSTATE;
        if (state == 0x13 || state == 0x15) {  // TX_UNDERFLOW o error
            RFST = 0x01;   // SIDLE
            delay_ms(10);
            RFST = 0x3A;   // SFTX
            delay_ms(10);
            LED_R_ON;
            LED_B_OFF;
            delay_ms(10);
            LED_R_OFF;
            return false;
        }
        
        timeout--;
        delay_ms(1);
    }
    
    // Timeout ocurrió
    RFST = 0x01;   // SIDLE
    delay_ms(10);
    LED_R_ON;
    LED_B_OFF;
    delay_ms(10);
    LED_R_OFF;
    return false;
}

bool rf_packet_available(void) {
    return packet_received;
}

bool rf_read_packet(RFPacket *packet) {
    if (!packet_received) {
        return false;
    }
    
    HAL_CRITICAL_STATEMENT({
        packet->length = rx_packet.length;
        for (uint8_t i = 0; i < rx_packet.length; i++) {
            packet->data[i] = rx_packet.data[i];
        }
        packet_received = false;
    });
    
    return true;
}

int8_t rf_get_rssi(void) {
    // RSSI offset de aproximadamente 73dB
    return ((int8_t) RSSI) - 73;
}

uint8_t rf_get_lqi(void) {
    return LQI;
}

#endif //0