#ifndef EPD_H
#define EPD_H

#include <stdint.h>

#ifdef PANEL_42
#define SCR_W 400
#define SCR_H 300
#define PANEL_TYPE 1
#else
#define SCR_W 296
#define SCR_H 152
#define PANEL_TYPE 0
#endif

#define WHITE 0
#define BLACK 1
#define RED   2

void epd_init(void);
void epd_wake(void);   /* re-power + configure panel (keeps draw list) */
void epd_clear(uint8_t color);
void epd_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color);
void epd_text(uint16_t x, uint16_t y, const char *s, uint8_t color, uint8_t scale);
void epd_icon(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
              const uint8_t __code *bmp, uint8_t color);
void epd_update(void);
void epd_sleep(void);

/* Full-frame streaming: begin, feed RLE token bytes ([runlen][value] pairs)
 * for the B/W plane then the red plane, then show. Lets the gateway send a
 * complete rendered image without the CC needing a framebuffer. */
void epd_img_begin(void);
void epd_img_rle(const uint8_t *buf, uint8_t n);
uint8_t epd_img_complete(void);   /* 1 = both planes fully streamed */
void epd_img_show(void);

#endif
