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
    uint32_t total_ticks = (uint32_t)minutes * 15360UL; // 1 min ≈ 15360 ticks @ 32.768kHz/128
    if (minutes == 0) {
        return;
    }

    // Detener Timer3 y guardar la configuración original del reloj.
    T3CTL = 0x00;
    uint8_t old_clkcon = CLKCON;

    uint32_t remaining_ticks = total_ticks;
    while (remaining_ticks > 0) {
        uint16_t current_ticks = (remaining_ticks > 65535) ? 65535 : (uint16_t)remaining_ticks;
        remaining_ticks -= current_ticks;

        // Establecer el reloj del sistema en el oscilador RC de 32 kHz. Esto se hace en cada iteración
        // del bucle porque CLKCON se restablece al despertar de PM2.
        CLKCON = (old_clkcon & ~0x07) | 0x02; // Usar oscilador RC de 32kHz (CLKSRC=10)

        // Configurar Timer1 para el despertar
        T1CTL = 0x0E;     // Preescalador 1:128, modo módulo
        T1CNTL = 0x00;    // Reiniciar temporizador
        T1CCTL0 = 0x44;   // Habilitar interrupción en comparación
        T1CC0L = (uint8_t)(current_ticks & 0xFF);
        T1CC0H = (uint8_t)(current_ticks >> 8);
        IRCON &= ~0x02;   // Limpiar la bandera de interrupción de Timer1

        // Habilitar la interrupción de Timer1
        IEN0 |= 0x02;
        // Las interrupciones globales deben estar habilitadas para despertar del sueño
        EA = 1;

        // Entrar en Modo de Potencia 2 (PM2) apagando el oscilador principal.
        // SLEEP.MODE = 2 (PM2), SLEEP.OSC_PD = 1 (Apagar oscilador de cristal)
        // Esto corresponde al valor 0x06. Es una asignación directa, no un OR.
        SLEEP = 0x06;
        PCON |= 0x01;

        // La hoja de datos requiere una secuencia específica para asegurar que el dispositivo
        // entre en modo de suspensión antes de que la CPU ejecute la siguiente instrucción.
        NOP();
        NOP();
        NOP();

        // --- PUNTO DE DESPERTAR ---
        // La CPU reanuda la ejecución aquí después de la interrupción del Timer1.
        // Al despertar de PM2, el oscilador RC de alta velocidad está funcionando.
        // Las interrupciones globales (EA) son deshabilitadas por el hardware.
    }

    // Restaurar la configuración original del reloj
    CLKCON = old_clkcon;
    // Esperar a que el oscilador de cristal se estabilice si es la fuente de reloj
    while (!(CLKCON & 0x40));

    // Reiniciar Timer3 y rehabilitar las interrupciones globales
    time_init();
    EA = 1;
}
