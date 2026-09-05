#ifndef GRAPH_H
#define GRAPH_H

#include "bmp.h"
#include <stddef.h>

typedef enum {
    GRAPH_BUILD_OK = 0,
    GRAPH_BUILD_ERR_INVALID_IMAGE,
    GRAPH_BUILD_ERR_MEMORY,
    GRAPH_BUILD_ERR_ADJ_TOO_LARGE
} GraphBuildStatus;

typedef struct {
    int region_count;
    int** adj;        // матрица смежности
    int* region_map;  // для каждого пикселя его region_id (width * height)
} Graph;

Graph* build_graph(const Image* img);
void free_graph(Graph* g);
GraphBuildStatus get_last_graph_build_status(void);
size_t get_last_graph_adj_matrix_bytes(void);
int get_last_graph_region_count(void);
size_t get_graph_adj_memory_limit_bytes(void);

#endif
