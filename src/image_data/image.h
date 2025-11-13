#ifndef IMAGE_DATA_H
#define IMAGE_DATA_H

#include <stdint.h>

#include "../display/epd.h"
/*
#if (EPD_SIZE == 213)
extern const unsigned char image_213_blackBuffer[];
extern const unsigned char image_213_redBuffer[];
#define lory_blackBuffer      ((uint8_t *) & image_213_blackBuffer)
#define lory_redBuffer        ((uint8_t *) & image_213_redBuffer)
#elif (EPD_SIZE == 266)
extern const unsigned char image_266_blackBuffer[];
extern const unsigned char image_266_redBuffer[];
#define lory_blackBuffer      ((uint8_t *) & image_266_blackBuffer)
#define lory_redBuffer        ((uint8_t *) & image_266_redBuffer)
#elif (EPD_SIZE == 417)
extern const unsigned char image_417_blackBuffer[];
extern const unsigned char image_417_redBuffer[];
#define lory_blackBuffer      ((uint8_t *) & image_417_blackBuffer)
#define lory_redBuffer        ((uint8_t *) & image_417_redBuffer)
#endif
*/

extern const unsigned char image_1[];
#define image1      ((uint8_t *) & image_1)
extern const unsigned char image_2[];
#define image2       ((uint8_t *) & image_2)

#endif // IMAGE_DATA_H
