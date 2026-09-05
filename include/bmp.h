#ifndef BMP_H
#define BMP_H

#include <stdint.h>

// union чтобы к пикселю можно было обращаться и по имени канала, и по индексу
typedef union {
    struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };
    uint8_t channels[3]; // удобно когда надо перебрать каналы в цикле
} Pixel;

typedef struct {
    unsigned int width;
    int height;
    Pixel* data;
} Image;

Image* read_bmp(const char* filename);
int write_bmp(const char* filename, const Image* img);
void free_image(Image* img);
Pixel get_pixel(const Image* img, int x, int y);

#endif
