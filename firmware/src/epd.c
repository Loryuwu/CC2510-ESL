#include <cc2510fx.h>
#include "epd.h"

#define BV(x) (1 << (x))

#define EPD_PWR   P0_0
#define EPD_CS    P0_1
#define EPD_DC    P1_2
#define EPD_BUSY  P1_3
#define EPD_RESET P2_0

#ifdef PANEL_42
#define HW_W   400
#define HW_H   300
#define HBYTES 50
#else
#define HW_W   152
#define HW_H   296
#define HBYTES 19
#endif

// ========== 5x7 Font (ASCII 32-126, column-major, bit0=top) ==========
static const __code uint8_t font5x7[95 * 5] = {
    0x00,0x00,0x00,0x00,0x00, // 32 space
    0x00,0x00,0x5F,0x00,0x00, // 33 !
    0x00,0x07,0x00,0x07,0x00, // 34 "
    0x14,0x7F,0x14,0x7F,0x14, // 35 #
    0x24,0x2A,0x7F,0x2A,0x12, // 36 $
    0x23,0x13,0x08,0x64,0x62, // 37 %
    0x36,0x49,0x55,0x22,0x50, // 38 &
    0x00,0x05,0x03,0x00,0x00, // 39 '
    0x00,0x1C,0x22,0x41,0x00, // 40 (
    0x00,0x41,0x22,0x1C,0x00, // 41 )
    0x08,0x2A,0x1C,0x2A,0x08, // 42 *
    0x08,0x08,0x3E,0x08,0x08, // 43 +
    0x00,0x50,0x30,0x00,0x00, // 44 ,
    0x08,0x08,0x08,0x08,0x08, // 45 -
    0x00,0x60,0x60,0x00,0x00, // 46 .
    0x20,0x10,0x08,0x04,0x02, // 47 /
    0x3E,0x51,0x49,0x45,0x3E, // 48 0
    0x00,0x42,0x7F,0x40,0x00, // 49 1
    0x42,0x61,0x51,0x49,0x46, // 50 2
    0x21,0x41,0x45,0x4B,0x31, // 51 3
    0x18,0x14,0x12,0x7F,0x10, // 52 4
    0x27,0x45,0x45,0x45,0x39, // 53 5
    0x3C,0x4A,0x49,0x49,0x30, // 54 6
    0x01,0x71,0x09,0x05,0x03, // 55 7
    0x36,0x49,0x49,0x49,0x36, // 56 8
    0x06,0x49,0x49,0x29,0x1E, // 57 9
    0x00,0x36,0x36,0x00,0x00, // 58 :
    0x00,0x56,0x36,0x00,0x00, // 59 ;
    0x00,0x08,0x14,0x22,0x41, // 60 <
    0x14,0x14,0x14,0x14,0x14, // 61 =
    0x41,0x22,0x14,0x08,0x00, // 62 >
    0x02,0x01,0x51,0x09,0x06, // 63 ?
    0x32,0x49,0x79,0x41,0x3E, // 64 @
    0x7E,0x11,0x11,0x11,0x7E, // 65 A
    0x7F,0x49,0x49,0x49,0x36, // 66 B
    0x3E,0x41,0x41,0x41,0x22, // 67 C
    0x7F,0x41,0x41,0x22,0x1C, // 68 D
    0x7F,0x49,0x49,0x49,0x41, // 69 E
    0x7F,0x09,0x09,0x09,0x01, // 70 F
    0x3E,0x41,0x41,0x51,0x32, // 71 G
    0x7F,0x08,0x08,0x08,0x7F, // 72 H
    0x00,0x41,0x7F,0x41,0x00, // 73 I
    0x20,0x40,0x41,0x3F,0x01, // 74 J
    0x7F,0x08,0x14,0x22,0x41, // 75 K
    0x7F,0x40,0x40,0x40,0x40, // 76 L
    0x7F,0x02,0x04,0x02,0x7F, // 77 M
    0x7F,0x04,0x08,0x10,0x7F, // 78 N
    0x3E,0x41,0x41,0x41,0x3E, // 79 O
    0x7F,0x09,0x09,0x09,0x06, // 80 P
    0x3E,0x41,0x51,0x21,0x5E, // 81 Q
    0x7F,0x09,0x19,0x29,0x46, // 82 R
    0x46,0x49,0x49,0x49,0x31, // 83 S
    0x01,0x01,0x7F,0x01,0x01, // 84 T
    0x3F,0x40,0x40,0x40,0x3F, // 85 U
    0x1F,0x20,0x40,0x20,0x1F, // 86 V
    0x3F,0x40,0x38,0x40,0x3F, // 87 W
    0x63,0x14,0x08,0x14,0x63, // 88 X
    0x07,0x08,0x70,0x08,0x07, // 89 Y
    0x61,0x51,0x49,0x45,0x43, // 90 Z
    0x00,0x7F,0x41,0x41,0x00, // 91 [
    0x02,0x04,0x08,0x10,0x20, // 92 backslash
    0x00,0x41,0x41,0x7F,0x00, // 93 ]
    0x04,0x02,0x01,0x02,0x04, // 94 ^
    0x40,0x40,0x40,0x40,0x40, // 95 _
    0x00,0x01,0x02,0x04,0x00, // 96 `
    0x20,0x54,0x54,0x54,0x78, // 97 a
    0x7F,0x48,0x44,0x44,0x38, // 98 b
    0x38,0x44,0x44,0x44,0x20, // 99 c
    0x38,0x44,0x44,0x48,0x7F, // 100 d
    0x38,0x54,0x54,0x54,0x18, // 101 e
    0x08,0x7E,0x09,0x01,0x02, // 102 f
    0x08,0x14,0x54,0x54,0x3C, // 103 g
    0x7F,0x08,0x04,0x04,0x78, // 104 h
    0x00,0x44,0x7D,0x40,0x00, // 105 i
    0x20,0x40,0x44,0x3D,0x00, // 106 j
    0x7F,0x10,0x28,0x44,0x00, // 107 k
    0x00,0x41,0x7F,0x40,0x00, // 108 l
    0x7C,0x04,0x18,0x04,0x78, // 109 m
    0x7C,0x08,0x04,0x04,0x78, // 110 n
    0x38,0x44,0x44,0x44,0x38, // 111 o
    0x7C,0x14,0x14,0x14,0x08, // 112 p
    0x08,0x14,0x14,0x18,0x7C, // 113 q
    0x7C,0x08,0x04,0x04,0x08, // 114 r
    0x48,0x54,0x54,0x54,0x20, // 115 s
    0x04,0x3F,0x44,0x40,0x20, // 116 t
    0x3C,0x40,0x40,0x20,0x7C, // 117 u
    0x1C,0x20,0x40,0x20,0x1C, // 118 v
    0x3C,0x40,0x30,0x40,0x3C, // 119 w
    0x44,0x28,0x10,0x28,0x44, // 120 x
    0x0C,0x50,0x50,0x50,0x3C, // 121 y
    0x44,0x64,0x54,0x4C,0x44, // 122 z
    0x00,0x08,0x36,0x41,0x00, // 123 {
    0x00,0x00,0x7F,0x00,0x00, // 124 |
    0x00,0x41,0x36,0x08,0x00, // 125 }
    0x08,0x04,0x08,0x10,0x08, // 126 ~
};

// ========== Draw List ==========
#define MAX_ITEMS 20
#define ITEM_RECT 1
#define ITEM_TEXT 2
#define ITEM_ICON 3

typedef struct {
    uint8_t type;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint8_t color;
    const char *text;
    uint8_t scale;
    const uint8_t __code *bmp;   /* ITEM_ICON: 1bpp row-major bitmap */
} DrawItem;

static __xdata DrawItem items[MAX_ITEMS];
static uint8_t item_count;
static uint8_t bg_color;

// ========== Delay ==========
void delay_ms(uint16_t ms) {
    uint16_t i;
    while (ms--) {
        for (i = 0; i < 1600; i++)
            __asm nop __endasm;
    }
}

// ========== SPI ==========
static void spi_tx(uint8_t d) {
    U0DBUF = d;
    while (U0CSR & 0x01) {}
}

static void cmd(uint8_t c) {
    EPD_DC = 0;
    EPD_CS = 0;
    spi_tx(c);
    EPD_CS = 1;
    EPD_DC = 1;
}

static void dat(uint8_t d) {
    EPD_CS = 0;
    spi_tx(d);
    EPD_CS = 1;
}

static uint8_t wait_busy(void) {
    uint16_t t = 30000;
    while (EPD_BUSY == 0 && --t)
        delay_ms(1);
    return t > 0;
}

// ========== Pixel Rendering ==========
static uint8_t font_pixel(const char *s, uint8_t scale,
                          uint16_t px, uint8_t py) {
    uint8_t pitch = 6 * scale;
    uint8_t ci, ic, fc, fr;
    uint8_t ch;
    uint8_t idx;

    ci = (uint8_t)(px / pitch);
    idx = 0;
    while (idx < ci && s[idx]) idx++;
    if (!s[idx]) return 0;

    ic = (uint8_t)(px % pitch);
    if (ic >= 5 * scale) return 0;

    fc = ic / scale;
    fr = py / scale;
    if (fr >= 7) return 0;

    ch = s[idx];
    if (ch < 32 || ch > 126) return 0;

    return (font5x7[(ch - 32) * 5 + fc] >> fr) & 1;
}

static uint8_t get_color(uint16_t sx, uint16_t sy) {
    uint8_t color = bg_color;
    uint8_t i;

    for (i = 0; i < item_count; i++) {
        DrawItem *it = &items[i];
        if (sx < it->x || sy < it->y) continue;
        if (sx >= it->x + it->w || sy >= it->y + it->h) continue;

        if (it->type == ITEM_RECT) {
            color = it->color;
        } else if (it->type == ITEM_TEXT) {
            uint16_t px = sx - it->x;
            uint8_t py = sy - it->y;
            if (font_pixel(it->text, it->scale, px, py))
                color = it->color;
        } else if (it->type == ITEM_ICON) {
            uint16_t col = sx - it->x;
            uint16_t row = sy - it->y;
            uint8_t  bpr = (uint8_t)((it->w + 7) >> 3);
            uint8_t  b = it->bmp[row * bpr + (col >> 3)];
            if (b & (0x80 >> (col & 7)))
                color = it->color;
        }
    }
    return color;
}

// ========== Public API ==========

/*
 * Power up and configure the panel hardware WITHOUT touching the draw list.
 * Use this to wake the panel from deep sleep (epd_sleep) before epd_update().
 * Leaving the panel powered between updates makes 3-colour e-ink smear (the
 * red pigment drifts), so the normal cycle is: epd_wake -> draw -> epd_update
 * -> epd_sleep.
 */
void epd_wake(void) {
    P0DIR |= BV(0) | BV(1) | BV(3) | BV(5);
    P1DIR |= BV(2);
    P1DIR &= ~BV(3);
    P2DIR |= BV(0);

    PERCFG &= ~0x01;
    U0CSR = 0;
    U0GCR = BV(5) | 17;
    U0BAUD = 0;
    U0CSR |= BV(6);
    P0SEL |= BV(3) | BV(5);

    EPD_CS = 1;
    EPD_PWR = 0;
    delay_ms(200);

    EPD_RESET = 0;
    delay_ms(10);
    EPD_RESET = 1;
    delay_ms(10);

    cmd(0x06);
    dat(0x17); dat(0x17); dat(0x17);

    cmd(0x04);
    wait_busy();

#ifdef PANEL_42
    /* IL0398 (4.2" 400x300 BWR): 1-byte panel setting, 4-byte resolution */
    cmd(0x00);
    dat(0x0F);

    cmd(0x61);
    dat((uint8_t)(HW_W >> 8)); dat((uint8_t)(HW_W & 0xFF));
    dat((uint8_t)(HW_H >> 8)); dat((uint8_t)(HW_H & 0xFF));
#else
    /* IL0373 (2.66" 296x152 BWR): 2-byte panel setting, 3-byte resolution */
    cmd(0x00);
    dat(0x0F); dat(0x0D);

    cmd(0x61);
    dat(HW_W); dat(HW_H >> 8); dat(HW_H & 0xFF);
#endif

    cmd(0x50);
    dat(0x77);
}

void epd_init(void) {
    epd_wake();
    item_count = 0;
    bg_color = WHITE;
}

void epd_clear(uint8_t color) {
    bg_color = color;
    item_count = 0;
}

void epd_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color) {
    DrawItem *it;
    if (item_count >= MAX_ITEMS) return;
    it = &items[item_count++];
    it->type = ITEM_RECT;
    it->x = x;
    it->y = y;
    it->w = w;
    it->h = h;
    it->color = color;
    it->text = (void *)0;
    it->scale = 0;
}

void epd_text(uint16_t x, uint16_t y, const char *s, uint8_t color, uint8_t scale) {
    DrawItem *it;
    uint8_t len = 0;
    const char *p = s;
    if (item_count >= MAX_ITEMS) return;
    while (*p++) len++;
    it = &items[item_count++];
    it->type = ITEM_TEXT;
    it->x = x;
    it->y = y;
    it->w = (uint16_t)len * 6 * scale;
    it->h = 7 * scale;
    it->color = color;
    it->text = s;
    it->scale = scale;
}

void epd_icon(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
              const uint8_t __code *bmp, uint8_t color) {
    DrawItem *it;
    if (item_count >= MAX_ITEMS) return;
    it = &items[item_count++];
    it->type = ITEM_ICON;
    it->x = x;
    it->y = y;
    it->w = w;
    it->h = h;
    it->color = color;
    it->text = (void *)0;
    it->scale = 0;
    it->bmp = bmp;
}

void epd_update(void) {
    uint16_t r;
    uint8_t bc, bit;

    // B/W channel
    cmd(0x10);
    for (r = 0; r < HW_H; r++) {
        for (bc = 0; bc < HBYTES; bc++) {
            uint8_t val = 0xFF;
            for (bit = 0; bit < 8; bit++) {
#ifdef PANEL_42
                uint16_t sx = (uint16_t)(bc * 8 + bit);
                uint16_t sy = r;
#else
                uint16_t sx = r;
                uint16_t sy = (uint16_t)((HW_W - 1) - (bc * 8 + bit));
#endif
                if (get_color(sx, sy) == BLACK)
                    val &= ~(0x80 >> bit);
            }
            dat(val);
        }
    }

    // Red channel
    cmd(0x13);
    for (r = 0; r < HW_H; r++) {
        for (bc = 0; bc < HBYTES; bc++) {
            uint8_t val = 0xFF;
            for (bit = 0; bit < 8; bit++) {
#ifdef PANEL_42
                uint16_t sx = (uint16_t)(bc * 8 + bit);
                uint16_t sy = r;
#else
                uint16_t sx = r;
                uint16_t sy = (uint16_t)((HW_W - 1) - (bc * 8 + bit));
#endif
                if (get_color(sx, sy) == RED)
                    val &= ~(0x80 >> bit);
            }
            dat(val);
        }
    }

    // Refresh
    cmd(0x12);
    delay_ms(100);
    wait_busy();
}

// ========== Full-frame streaming (RLE) ==========
// The gateway renders the whole image and streams it here RLE-compressed, so
// the CC needs no framebuffer (only ~4KB RAM) and never needs re-flashing for
// new content. Bytes are written straight into the panel RAM as they arrive:
// first the B/W plane (PLANE_BYTES bytes after cmd 0x10), then the red plane
// (after cmd 0x13), then a refresh. RLE token stream = [runlen][value] pairs.

#define PLANE_BYTES ((uint16_t)HW_H * HBYTES)   // 296*19 = 5624 per plane

static uint16_t img_pos;
static uint8_t  rle_have_len;   // did we read a run-length byte yet?
static uint8_t  rle_run;        // pending run length

static void img_putbyte(uint8_t v) {
    if (img_pos == PLANE_BYTES) cmd(0x13);       // boundary -> red plane
    if (img_pos < 2 * PLANE_BYTES) { dat(v); img_pos++; }
}

void epd_img_begin(void) {
    epd_wake();
    cmd(0x10);                 // start B/W plane
    img_pos = 0;
    rle_have_len = 0;
}

void epd_img_rle(const uint8_t *buf, uint8_t n) {
    uint8_t i, k;
    for (i = 0; i < n; i++) {
        if (!rle_have_len) {
            rle_run = buf[i];
            rle_have_len = 1;
        } else {
            for (k = 0; k < rle_run; k++) img_putbyte(buf[i]);
            rle_have_len = 0;
        }
    }
}

/* True once both planes (2 * PLANE_BYTES) have been streamed in full. Lets
 * the caller refuse to refresh a truncated frame (keep the old image instead
 * of showing a half-written one). */
uint8_t epd_img_complete(void) {
    return img_pos == 2 * PLANE_BYTES;
}

void epd_img_show(void) {
    cmd(0x12);                 // refresh
    delay_ms(100);
    wait_busy();
    epd_sleep();               // deep sleep -> no red smear, image retained
}

void epd_sleep(void) {
    cmd(0x02);
    wait_busy();
    cmd(0x07);
    dat(0xA5);
    EPD_PWR = 1;
}
