#include "bmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

// выравниваем структуры заголовков BMP строго побайтно, без padding компилятора
#pragma pack(push, 1)

typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BITMAPFILEHEADER;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BITMAPINFOHEADER;

#pragma pack(pop)

static int inside(const Image* img, int x, int y) {
    return img && x >= 0 && x < (int)img->width && y >= 0 && y < img->height;
}

Image* read_bmp(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Cannot open file: %s\n", filename);
        return NULL;
    }

    BITMAPFILEHEADER file_header;
    BITMAPINFOHEADER info_header;

    if (fread(&file_header, sizeof(file_header), 1, file) != 1 ||
        fread(&info_header, sizeof(info_header), 1, file) != 1) {
        fclose(file);
        return NULL;
    }

    if (file_header.bfType != 0x4D42) {
        printf("File is not BMP\n");
        fclose(file);
        return NULL;
    }

    // базовая валидация заголовков, чтобы не читать битый BMP
    if (info_header.biSize < sizeof(BITMAPINFOHEADER) ||
        info_header.biPlanes != 1 ||
        info_header.biBitCount != 24 ||
        info_header.biCompression != 0) {
        printf("Unsupported BMP format\n");
        fclose(file);
        return NULL;
    }

    if (file_header.bfOffBits < (uint32_t)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER))) {
        printf("Invalid BMP pixel data offset\n");
        fclose(file);
        return NULL;
    }

    if (info_header.biWidth <= 0 || info_header.biHeight == 0 || info_header.biHeight == INT_MIN) {
        printf("Invalid BMP dimensions\n");
        fclose(file);
        return NULL;
    }

    Image* img = (Image*)malloc(sizeof(Image));
    if (!img) {
        fclose(file);
        return NULL;
    }

    img->width = (unsigned int)info_header.biWidth;
    img->height = (info_header.biHeight > 0) ? info_header.biHeight : -info_header.biHeight;

    // защита от переполнений в вычислениях размеров буферов
    if ((size_t)img->width > SIZE_MAX / (size_t)img->height) {
        free(img);
        fclose(file);
        return NULL;
    }
    size_t pixel_count = (size_t)img->width * (size_t)img->height;
    if (pixel_count > SIZE_MAX / sizeof(Pixel)) {
        free(img);
        fclose(file);
        return NULL;
    }
    if (img->width > (unsigned int)(INT_MAX / 3) || ((int)img->width * 3) > (INT_MAX - 3)) {
        free(img);
        fclose(file);
        return NULL;
    }

    img->data = (Pixel*)malloc(pixel_count * sizeof(Pixel));
    if (!img->data) {
        free(img);
        fclose(file);
        return NULL;
    }

    int row_size = ((int)img->width * 3 + 3) & ~3;
    unsigned char* row = (unsigned char*)malloc((size_t)row_size);
    if (!row) {
        free(img->data);
        free(img);
        fclose(file);
        return NULL;
    }

    if (fseek(file, (long)file_header.bfOffBits, SEEK_SET) != 0) {
        free(row);
        free(img->data);
        free(img);
        fclose(file);
        return NULL;
    }

    for (int y = 0; y < img->height; y++) {
        if (fread(row, 1, (size_t)row_size, file) != (size_t)row_size) {
            free(row);
            free(img->data);
            free(img);
            fclose(file);
            return NULL;
        }

        int real_y = info_header.biHeight > 0 ? (img->height - 1 - y) : y;
        for (int x = 0; x < (int)img->width; x++) {
            img->data[real_y * (int)img->width + x].b = row[x * 3 + 0];
            img->data[real_y * (int)img->width + x].g = row[x * 3 + 1];
            img->data[real_y * (int)img->width + x].r = row[x * 3 + 2];
        }
    }

    free(row);
    fclose(file);
    return img;
}

int write_bmp(const char* filename, const Image* img) {
    if (!filename || !img || !img->data || img->width == 0 || img->height <= 0) {
        return 0;
    }

    FILE* file = fopen(filename, "wb");
    if (!file) {
        printf("Cannot open file for write: %s\n", filename);
        return 0;
    }

    const int width = (int)img->width;
    const int height = img->height;
    const int row_size = (width * 3 + 3) & ~3;
    const uint32_t image_size = (uint32_t)(row_size * height);

    BITMAPFILEHEADER file_header;
    file_header.bfType = 0x4D42;
    file_header.bfSize = (uint32_t)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + image_size);
    file_header.bfReserved1 = 0;
    file_header.bfReserved2 = 0;
    file_header.bfOffBits = (uint32_t)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER));

    BITMAPINFOHEADER info_header;
    info_header.biSize = sizeof(BITMAPINFOHEADER);
    info_header.biWidth = width;
    info_header.biHeight = height;
    info_header.biPlanes = 1;
    info_header.biBitCount = 24;
    info_header.biCompression = 0;
    info_header.biSizeImage = image_size;
    info_header.biXPelsPerMeter = 0;
    info_header.biYPelsPerMeter = 0;
    info_header.biClrUsed = 0;
    info_header.biClrImportant = 0;

    if (fwrite(&file_header, sizeof(file_header), 1, file) != 1 ||
        fwrite(&info_header, sizeof(info_header), 1, file) != 1) {
        fclose(file);
        return 0;
    }

    unsigned char* row = (unsigned char*)calloc((size_t)row_size, 1);
    if (!row) {
        fclose(file);
        return 0;
    }

    for (int y = 0; y < height; y++) {
        int src_y = height - 1 - y;
        memset(row, 0, (size_t)row_size);

        for (int x = 0; x < width; x++) {
            Pixel p = img->data[src_y * width + x];
            row[x * 3 + 0] = p.b;
            row[x * 3 + 1] = p.g;
            row[x * 3 + 2] = p.r;
        }

        if (fwrite(row, 1, (size_t)row_size, file) != (size_t)row_size) {
            free(row);
            fclose(file);
            return 0;
        }
    }

    free(row);
    fclose(file);
    return 1;
}

void free_image(Image* img) {
    if (!img) return;
    free(img->data);
    free(img);
}

Pixel get_pixel(const Image* img, int x, int y) {
    Pixel black = { .r = 0, .g = 0, .b = 0 };
    if (!inside(img, x, y)) return black;
    return img->data[y * (int)img->width + x];
}
