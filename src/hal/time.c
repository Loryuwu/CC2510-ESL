#include "time.h"

static volatile uint32_t currentTime = 0;

/***************************
 *Timer3: genera millis()
 ***************************/

INTERRUPT(timer3_isr, T3_VECTOR) {
  T3OVFIF = 0; // clear overflow flag
  currentTime += 1;
}

void time_init() {
  T3CTL = (uint8_t)((0b111 << 5) | BV(1) | BV(3)); // clk/128, modulo mode, overflow interrupt en
  T3CC0 = 203;                                     // ~1ms intrerval
  T3IE = BV(0);                                    // enable timer3 interrupt
  T3CTL |= BV(4);                                  // start Timer 3
}

volatile uint32_t millis() {
  uint32_t value;
  HAL_CRITICAL_STATEMENT(value = currentTime);
  return value;
}

void delay_ms(uint16_t milliseconds) {
  uint32_t start = millis();
  while (millis() - start < milliseconds) {
  }
}

/***************************
 *Timer1: sleep_for_minutes()
 ***************************/

INTERRUPT(timer1_isr, T1_VECTOR) {
  IRCON &= ~0x02; // limpia flag
}

void sleep_for_minutes(uint16_t minutes) {
    uint32_t total_ticks = (uint32_t)minutes * 15360UL; // 1 min ≈ 15.360 ticks a 32kHz/128
    uint32_t remaining_ticks = total_ticks;
    
    // Apagar Timer3 (ahorro)
    T3CTL = 0x00;
    T3OVFIF = 0;               // limpiar flag previo

    // Asegurar oscilador de 32 kHz activo
    CLKCON |= 0x01;              // selecciona 32 kHz como fuente
    while (!(CLKCON & 0x40));     // esperar a que esté estable


    while (remaining_ticks > 0) {
        uint16_t current_ticks = (remaining_ticks > 65535) ? 65535 : (uint16_t)remaining_ticks;

        // Configurar Timer1
        T1CTL = 0x0E;                // div 128, modo módulo
        T1CNTL = 0x00;
        T1CCTL0 = 0x44;              // comparación + interrupción
        T1CC0L = (uint8_t)(current_ticks & 0xFF);
        T1CC0H = (uint8_t)(current_ticks >> 8);
        IRCON &= ~0x02;              // limpia flag previo

        // Habilitar interrupciones globales
        IEN0 |= 0x02;               // habilitar int Timer1
        EA = 1;

        // Entrar en PM2
        SLEEP = 0x06;
        PCON |= 0x01;                // sleep
        
        remaining_ticks -= current_ticks;
    }

    // --- Al despertar ---
    // Restaurar el reloj principal
    CLKCON &= ~0x07;             // selecciona el oscilador principal
    while (!(CLKCON & 0x40));    // esperar a que esté estable
    time_init();                 // reactivar timer principal
}
