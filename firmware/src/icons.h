#ifndef ICONS_H
#define ICONS_H
#include <stdint.h>

#define ICON_W 48
#define ICON_H 48

#define ICON_SUN 0
#define ICON_CLOUD 1
#define ICON_RAIN 2
#define ICON_SNOW 3
#define ICON_PARTLY 4
#define ICON_FOG 5

const __code uint8_t *icon_get(uint8_t id, uint8_t *w, uint8_t *h);

#endif
