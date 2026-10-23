/*
 * main_flash_test.c  —  Demo fuer das eingebundene SPI-Flash-Modul.
 *
 * Initialisiert den externen Winbond-Flash (USART1 SPI), liest die
 * Hersteller-ID und zeigt das Ergebnis auf dem E-Paper an. Belegt USART1
 * (Flash) + USART0 (EPD) - laeuft also NICHT zusammen mit UART/COBS.
 *
 * Build:  make flash-test   ->   flash_test.bin
 */
#include <cc2510fx.h>
#include "epd.h"
#include "flash.h"

static const __code char HEX[] = "0123456789ABCDEF";

void main(void) {
    char line[16];
    uint16_t id;
    uint8_t i = 0;

    spi_flash_init();
    id = spi_flash_read_id();

    line[i++] = 'I'; line[i++] = 'D'; line[i++] = ' ';
    line[i++] = '0'; line[i++] = 'x';
    line[i++] = HEX[(id >> 12) & 0xF];
    line[i++] = HEX[(id >>  8) & 0xF];
    line[i++] = HEX[(id >>  4) & 0xF];
    line[i++] = HEX[ id        & 0xF];
    line[i]   = 0;

    epd_clear(WHITE);
    epd_rect(0, 0, SCR_W, 24, RED);
    epd_text(70, 6, "FLASH TEST", WHITE, 2);
    if ((id >> 8) == 0xEF) epd_text(10, 40, "Winbond OK", BLACK, 2);
    else                   epd_text(10, 40, "kein Winbond", RED, 2);
    epd_text(10, 72, line, BLACK, 2);
    epd_wake();
    epd_update();
    epd_sleep();

    spi_flash_disable();
    for (;;) { }
}
