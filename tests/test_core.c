#include "bmp.h"
#include "dsatur.h"
#include "graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int coloring_is_valid(const Graph* g, const int* colors) {
    if (!g || !colors) return 0;

    for (int i = 0; i < g->region_count; ++i) {
        if (colors[i] <= 0 || colors[i] > 5) return 0;
        for (int j = i + 1; j < g->region_count; ++j) {
            if (g->adj[i][j] && colors[i] == colors[j]) return 0;
        }
    }

    return 1;
}

static int test_dsatur_k4(void) {
    Graph g = {0};
    g.region_count = 4;
    g.adj = (int**)calloc(4, sizeof(int*));
    if (!g.adj) return 0;

    for (int i = 0; i < 4; ++i) {
        g.adj[i] = (int*)calloc(4, sizeof(int));
        if (!g.adj[i]) return 0;
        for (int j = 0; j < 4; ++j) {
            if (i != j) g.adj[i][j] = 1;
        }
    }

    int* colors = dsatur(&g);
    int ok = coloring_is_valid(&g, colors);

    free(colors);
    for (int i = 0; i < 4; ++i) free(g.adj[i]);
    free(g.adj);
    return ok;
}

static int test_graph_from_image(void) {
    Image img = {0};
    img.width = 5;
    img.height = 3;
    img.data = (Pixel*)calloc((size_t)img.width * (size_t)img.height, sizeof(Pixel));
    if (!img.data) return 0;

    Pixel white = {255, 255, 255};
    Pixel black = {0, 0, 0};

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < (int)img.width; ++x) {
            img.data[y * (int)img.width + x] = (x == 2) ? black : white;
        }
    }

    Graph* g = build_graph(&img);
    int ok = g && g->region_count == 2 && g->adj[0][1] && g->adj[1][0];

    free_graph(g);
    free(img.data);
    return ok;
}

static int test_bmp_roundtrip(void) {
    const char* path = "map_core_roundtrip_test.bmp";
    Image img = {0};
    img.width = 2;
    img.height = 2;
    img.data = (Pixel*)calloc(4, sizeof(Pixel));
    if (!img.data) return 0;

    img.data[0] = (Pixel){255, 0, 0};
    img.data[1] = (Pixel){0, 255, 0};
    img.data[2] = (Pixel){0, 0, 255};
    img.data[3] = (Pixel){255, 255, 255};

    int ok = write_bmp(path, &img);
    Image* loaded = ok ? read_bmp(path) : NULL;

    if (!loaded || loaded->width != img.width || loaded->height != img.height ||
        memcmp(loaded->data, img.data, 4 * sizeof(Pixel)) != 0) {
        ok = 0;
    }

    free_image(loaded);
    free(img.data);
    remove(path);
    return ok;
}

int main(void) {
    if (!test_dsatur_k4()) {
        fprintf(stderr, "test_dsatur_k4 failed\n");
        return 1;
    }
    if (!test_graph_from_image()) {
        fprintf(stderr, "test_graph_from_image failed\n");
        return 1;
    }
    if (!test_bmp_roundtrip()) {
        fprintf(stderr, "test_bmp_roundtrip failed\n");
        return 1;
    }

    puts("All core tests passed");
    return 0;
}
