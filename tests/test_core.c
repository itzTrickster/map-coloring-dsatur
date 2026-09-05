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

static int max_color_used(const int* colors, int count) {
    int max_color = 0;
    for (int i = 0; i < count; ++i) {
        if (colors[i] > max_color) max_color = colors[i];
    }
    return max_color;
}

static int init_complete_graph(Graph* g, int count) {
    if (!g || count <= 0) return 0;

    memset(g, 0, sizeof(*g));
    g->region_count = count;
    g->adj = (int**)calloc((size_t)count, sizeof(int*));
    if (!g->adj) return 0;

    for (int i = 0; i < count; ++i) {
        g->adj[i] = (int*)calloc((size_t)count, sizeof(int));
        if (!g->adj[i]) {
            for (int j = 0; j < i; ++j) free(g->adj[j]);
            free(g->adj);
            g->adj = NULL;
            return 0;
        }

        for (int j = 0; j < count; ++j) {
            if (i != j) g->adj[i][j] = 1;
        }
    }

    return 1;
}

static void free_test_graph(Graph* g) {
    if (!g || !g->adj) return;

    for (int i = 0; i < g->region_count; ++i) free(g->adj[i]);
    free(g->adj);
    g->adj = NULL;
    g->region_count = 0;
}

static int test_dsatur_k4(void) {
    Graph g;
    if (!init_complete_graph(&g, 4)) return 0;

    int* colors = dsatur(&g);
    int ok = coloring_is_valid(&g, colors) && max_color_used(colors, 4) == 4;

    free(colors);
    free_test_graph(&g);
    return ok;
}

static int test_dsatur_k5_fallback(void) {
    Graph g;
    if (!init_complete_graph(&g, 5)) return 0;

    int* colors = dsatur(&g);
    int ok = coloring_is_valid(&g, colors) && max_color_used(colors, 5) == 5;

    free(colors);
    free_test_graph(&g);
    return ok;
}

static int test_dsatur_rejects_k6(void) {
    Graph g;
    if (!init_complete_graph(&g, 6)) return 0;

    int* colors = dsatur(&g);
    int ok = colors == NULL;

    free(colors);
    free_test_graph(&g);
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

static int test_missing_bmp(void) {
    Image* image = read_bmp("file_that_should_not_exist_7f2a4b.bmp");
    int ok = image == NULL;
    free_image(image);
    return ok;
}

int main(void) {
    if (!test_dsatur_k4()) {
        fprintf(stderr, "test_dsatur_k4 failed\n");
        return 1;
    }
    if (!test_dsatur_k5_fallback()) {
        fprintf(stderr, "test_dsatur_k5_fallback failed\n");
        return 1;
    }
    if (!test_dsatur_rejects_k6()) {
        fprintf(stderr, "test_dsatur_rejects_k6 failed\n");
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
    if (!test_missing_bmp()) {
        fprintf(stderr, "test_missing_bmp failed\n");
        return 1;
    }

    puts("All core tests passed");
    return 0;
}
