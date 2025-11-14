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
static uint32_t ms_to_ticks(uint32_t ms) {
    // Convierte milisegundos a ticks de Timer1 con reloj de 32.768kHz y prescaler 128
    // 1 tick = 128 / 32768 segundos = 3.90625 ms
    return (ms * 32768UL) / 1000UL;
}

INTERRUPT(timer1_isr, T1_VECTOR) {
  // T1STAT &= ~0x01; // Clear channel 0 interrupt flag
  IRCON &= ~0x80; // limpia flag
}

void wor_sleep_60s() {
    // Limpia banderas
    STIF = 0;
    WORIRQ = 0;     // Limpia flags internos del modulo WOR
    STIE = 1;

    // Prescaler máximo (512 Hz → 1.95 ms por tick)
    WORCTRL = (3 << 2);

    // Timeout de 60s:
    // 60 s / (1/512) = 30720 ticks  (~30768 por ajuste)
    uint16_t ticks = 61440;
    WOREVT0 = ticks & 0xFF;
    WOREVT1 = ticks >> 8;



    // Sincroniza el registro
    WORCTRL |= 0x01;   // EVENT0 reset

    STIF = 0;
    // Entra a PM2 (deep sleep)
    SLEEP = 0x06;
    __asm nop __endasm;
    __asm nop __endasm;
}

void sleep_ms(uint32_t ms) {

  uint32_t ticks = ms_to_ticks(ms);
  if (ticks == 0) return;
  if (ticks > 0xFFFFFFUL) ticks = 0xFFFFFFUL;

  // Deshabilitar interrupciones globales
  EA = 0;

  // ------------------------------
  // 1. Poner el valor objetivo ST0–ST2
  // ------------------------------
  // ST2 = (uint8_t)(ticks >> 16);
  // ST1 = (uint8_t)(ticks >> 8);
  // ST0 = (uint8_t)(ticks);

  // ------------------------------
  // 2. Limpiar bandera de interrupción
  // ------------------------------
  IRCON &= ~0x80; // STIF

  // ------------------------------
  // 3. Habilitar interrupción del Sleep Timer
  // ------------------------------
  IEN0 |= 0x20;   // STIE (bit 5)
  EA = 1;

  // ------------------------------
  // 4. Entrar en PM2
  // ------------------------------
  SLEEP &= ~0x03;     // limpiar PM bits
  SLEEP |=  0x02;     // PM2 = 0b10

  // Secuencia obligatoria para iniciar sleep
  PCON |= 0x01;       // CPUOFF
  __asm nop __endasm;
  __asm nop __endasm;
  __asm nop __endasm;

  // -----------------------------------------------
  // --- DESPERTAR ---
  // Cuando despierta, EA se deshabilita automáticamente.
  // -----------------------------------------------

  EA = 1; // restaurar interrupciones globales
  init_clock(); // reactivar el reloj principal
}

// void sleep_for_minutes_1(uint16_t minutes) {
//     uint32_t total_ticks = (uint32_t)minutes * 15360UL; // 1 min ≈ 15.360 ticks a 32kHz/128
//     uint32_t remaining_ticks = total_ticks;
    
//     // Apagar Timer3 (ahorro)
//     T3CTL = 0x00;
//     T3OVFIF = 0;               // limpiar flag previo

//     // Asegurar oscilador de 32 kHz activo
//     CLKCON |= 0x01;              // selecciona 32 kHz como fuente
//     while (!(CLKCON & 0x40));     // esperar a que esté estable


//     while (remaining_ticks > 0) {
//         uint16_t current_ticks = (remaining_ticks > 65535) ? 65535 : (uint16_t)remaining_ticks;

//         // Configurar Timer1
//         T1CTL = 0x0E;                // div 128, modo módulo
//         T1CNTL = 0x00;
//         T1CCTL0 = 0x44;              // comparación + interrupción
//         T1CC0L = (uint8_t)(current_ticks & 0xFF);
//         T1CC0H = (uint8_t)(current_ticks >> 8);
//         IRCON &= ~0x02;              // limpia flag previo

//         // Habilitar interrupciones globales
//         IEN0 |= 0x02;               // habilitar int Timer1
//         EA = 1;

//         // Entrar en PM2
//         SLEEP = 0x06;
//         PCON |= 0x01;                // sleep
        
//         remaining_ticks -= current_ticks;
//     }

//     // --- Al despertar ---
//     // Restaurar el reloj principal
//     CLKCON &= ~0x07;             // selecciona el oscilador principal
//     while (!(CLKCON & 0x40));    // esperar a que esté estable
//     time_init();                 // reactivar timer principal
// }

// void sleep_for_minutes_2(uint16_t minutes) {
//     uint32_t total_ticks = (uint32_t)minutes * 15360UL; // 1 min ≈ 15.360 ticks a 32.768kHz/128
//     uint32_t remaining_ticks = total_ticks;

//     // Apagar Timer3 (ahorro)
//     T3CTL = 0x00;
//     T3OVFIF = 0;

//     uint8_t old_clkcon = CLKCON; // Guardar configuración original del reloj

//     while (remaining_ticks > 0) {
//         uint16_t current_ticks = (remaining_ticks > 65535) ? 65535 : (uint16_t)remaining_ticks;

//         // Cambiar a oscilador de 32kHz RC para el ciclo de sueño.
//         // Se hace dentro del bucle porque CLKCON se resetea al despertar.
//         CLKCON = (CLKCON & ~0x0B) | 0x02; // CLKSRC=10 (32k), OSC32=0 (RC)

//         // Configurar Timer1
//         T1CTL = 0x0E;                // div 128, modo módulo
//         T1CNTL = 0x00;
//         T1CCTL0 = 0x44;              // comparación + interrupción
//         T1CC0L = (uint8_t)(current_ticks & 0xFF);
//         T1CC0H = (uint8_t)(current_ticks >> 8);
//         IRCON &= ~0x02;              // limpia flag previo

//         // Habilitar interrupciones
//         IEN0 |= 0x02;
//         EA = 1;

//         // Entrar en PM2
//         SLEEP = 0x06;
//         PCON |= 0x01;
//         NOP(); // ¡Crucial para la sincronización del despertar!

//         // --- Punto de despertar ---
//         // La CPU se está ejecutando ahora con el oscilador RC de alta velocidad (~13MHz) por defecto.
        
//         remaining_ticks -= current_ticks;
//     }

//     // --- Despertar final ---
//     // Restaurar el reloj principal
//     CLKCON = old_clkcon;
//     while (!(CLKCON & 0x40));    // esperar a que esté estable
//     time_init();                 // reactivar timer principal
// }

// void sleep_for_minutes(uint16_t minutes) {
//     if (minutes == 0) {
//         return;
//     }
//     uint32_t total_ticks = (uint32_t)minutes * 15360UL; // 1 min ≈ 15360 ticks @ 32.768kHz/128
//     uint8_t old_clkcon = CLKCON;

//     // Detener Timer3 y guardar la configuración original del reloj.
//     T3CTL = 0x00;
//     EA = 0; // Deshabilitar interrupciones globales

//     uint32_t remaining_ticks = total_ticks;
//     while (remaining_ticks > 0) {
//         uint16_t current_ticks = (remaining_ticks > 65535) ? 65535 : (uint16_t)remaining_ticks;
//         remaining_ticks -= current_ticks;

//         // Establecer el reloj del sistema en el oscilador RC de 32 kHz. Esto se hace en cada iteración
//         // del bucle porque CLKCON se restablece al despertar de PM2.
//         CLKCON = (old_clkcon & ~0x07) | 0x01; // Usar oscilador RC de 32kHz (CLKSRC=10)
//         while (!(SLEEP & 0x20)); // Esperar a que el oscilador esté estable
        

//         // Configurar Timer1 para el despertar
//         T1CTL = 0x0E;     // Preescalador 1:128, modo módulo
//         T1CNTL = 0x00;    // Reiniciar temporizador
//         T1CCTL0 = 0x44;   // Habilitar interrupción en comparación
//         T1CC0L = (uint8_t)(current_ticks & 0xFF);
//         T1CC0H = (uint8_t)(current_ticks >> 8);
//         // T1STAT &= ~0x01;
//         IRCON &= ~0x02;   // Limpiar la bandera de interrupción de Timer1

//         // Habilitar la interrupción de Timer1
//         IEN0 |= 0x02;
//         // Las interrupciones globales deben estar habilitadas para despertar del sueño
//         EA = 1;

//         // Apagar el oscilador de cristal principal y entrar en Modo de Potencia 2
//         SLEEP = (SLEEP & ~0x03) | 0x02; // SLEEP.OSC_PD = 1
//         SLEEP |= 0x04;
//         PCON |= 0x01;

//         // La hoja de datos requiere una secuencia específica para asegurar que el dispositivo
//         // entre en modo de suspensión antes de que la CPU ejecute la siguiente instrucción.
//         NOP();
//         NOP();
//         NOP();

//         // --- PUNTO DE DESPERTAR ---
//         // La CPU reanuda la ejecución aquí después de la interrupción del Timer1.
//         // Al despertar de PM2, el oscilador RC de alta velocidad está funcionando.
//         // Las interrupciones globales (EA) son deshabilitadas por el hardware.
//     }

//     // Restaurar la configuración original del reloj
//     CLKCON = old_clkcon;
//     // Esperar a que el oscilador de cristal se estabilice si es la fuente de reloj
//     while (!(CLKCON & 0x40));
    
//     // Reiniciar Timer3 y rehabilitar las interrupciones globales
//     time_init();
//     EA = 1;
// }

