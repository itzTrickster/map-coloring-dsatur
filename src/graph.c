#include "graph.h"
#include <limits.h>
#include <stdlib.h>

static GraphBuildStatus g_last_build_status = GRAPH_BUILD_OK;
static size_t g_last_adj_matrix_bytes = 0;
static int g_last_region_count = 0;

// ограничение на матрицу смежности, чтобы не уходить в десятки/сотни мегабайт
#define GRAPH_ADJ_MEMORY_LIMIT_BYTES (64u * 1024u * 1024u)

static void set_build_status(GraphBuildStatus status, size_t adj_bytes, int region_count) {
    g_last_build_status = status;
    g_last_adj_matrix_bytes = adj_bytes;
    g_last_region_count = region_count;
}

GraphBuildStatus get_last_graph_build_status(void) {
    return g_last_build_status;
}

size_t get_last_graph_adj_matrix_bytes(void) {
    return g_last_adj_matrix_bytes;
}

int get_last_graph_region_count(void) {
    return g_last_region_count;
}

size_t get_graph_adj_memory_limit_bytes(void) {
    return GRAPH_ADJ_MEMORY_LIMIT_BYTES;
}

// узел очереди BFS
typedef struct QueueNode {
    int x, y;
    struct QueueNode* next;
} QueueNode;

// добавляем пиксель в хвост очереди, возвращаем новый хвост или NULL при OOM
static QueueNode* enqueue(QueueNode* tail, int x, int y) {
    QueueNode* node = (QueueNode*)malloc(sizeof(QueueNode));
    if (!node) return NULL;
    node->x = x;
    node->y = y;
    node->next = NULL;
    if (tail) tail->next = node;
    return node;
}

// граничный пиксель = чёрный
static int is_border(Pixel p) {
    return (p.r == 0 && p.g == 0 && p.b == 0);
}

// индекс пикселя в 1D-массиве
static int idx(int x, int y, int w) {
    return y * w + x;
}

// симметрично отмечаем смежность двух регионов
static void mark_adjacent(int** adj, int id1, int id2) {
    if (!adj) return;
    if (id1 < 0 || id2 < 0 || id1 == id2) return;
    adj[id1][id2] = 1;
    adj[id2][id1] = 1;
}

Graph* build_graph(const Image* img) {
    set_build_status(GRAPH_BUILD_OK, 0, 0);
    if (!img || !img->data || img->width == 0 || img->height <= 0) {
        set_build_status(GRAPH_BUILD_ERR_INVALID_IMAGE, 0, 0);
        return NULL;
    }

    int w = (int)img->width;
    int h = img->height;
    if (w > INT_MAX / h) {
        set_build_status(GRAPH_BUILD_ERR_INVALID_IMAGE, 0, 0);
        return NULL;
    }
    int size = w * h;

    int* visited = (int*)calloc((size_t)size, sizeof(int));
    int* region_id = (int*)malloc((size_t)size * sizeof(int));
    if (!visited || !region_id) {
        free(visited);
        free(region_id);
        set_build_status(GRAPH_BUILD_ERR_MEMORY, 0, 0);
        return NULL;
    }

    for (int i = 0; i < size; i++) {
        region_id[i] = -1;
    }

    int region_count = 0;
    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    // первый проход: выделяем все белые связные области
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int start = idx(x, y, w);
            if (visited[start]) continue;

            Pixel p = get_pixel(img, x, y);
            if (is_border(p)) continue;

            QueueNode* head = (QueueNode*)malloc(sizeof(QueueNode));
            if (!head) {
                free(visited);
                free(region_id);
                set_build_status(GRAPH_BUILD_ERR_MEMORY, 0, region_count);
                return NULL;
            }
            head->x = x;
            head->y = y;
            head->next = NULL;
            QueueNode* tail = head;
            visited[start] = 1;
            int queue_alloc_failed = 0;

            while (head != NULL) {
                int cx = head->x;
                int cy = head->y;
                QueueNode* old = head;
                head = head->next;
                if (head == NULL) tail = NULL;
                free(old);

                region_id[idx(cx, cy, w)] = region_count;

                for (int k = 0; k < 4; k++) {
                    int nx = cx + dx[k];
                    int ny = cy + dy[k];

                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;

                    int nid = idx(nx, ny, w);
                    if (visited[nid]) continue;

                    Pixel np = get_pixel(img, nx, ny);
                    if (is_border(np)) continue;

                    QueueNode* new_tail = enqueue(tail, nx, ny);
                    if (!new_tail) {
                        queue_alloc_failed = 1;
                        break;
                    }
                    tail = new_tail;
                    if (head == NULL) head = tail;
                    visited[nid] = 1;
                }

                if (queue_alloc_failed) break;
            }

            if (queue_alloc_failed) {
                while (head != NULL) {
                    QueueNode* old = head;
                    head = head->next;
                    free(old);
                }
                free(visited);
                free(region_id);
                set_build_status(GRAPH_BUILD_ERR_MEMORY, 0, region_count);
                return NULL;
            }

            region_count++;
        }
    }

    size_t adj_total_bytes = 0;
    if (region_count > 0) {
        size_t rc = (size_t)region_count;
        if (rc > SIZE_MAX / sizeof(int*)) {
            free(visited);
            free(region_id);
            set_build_status(GRAPH_BUILD_ERR_ADJ_TOO_LARGE, SIZE_MAX, region_count);
            return NULL;
        }
        size_t adj_ptr_bytes = rc * sizeof(int*);

        if (rc > SIZE_MAX / rc) {
            free(visited);
            free(region_id);
            set_build_status(GRAPH_BUILD_ERR_ADJ_TOO_LARGE, SIZE_MAX, region_count);
            return NULL;
        }
        size_t adj_cell_count = rc * rc;
        if (adj_cell_count > SIZE_MAX / sizeof(int)) {
            free(visited);
            free(region_id);
            set_build_status(GRAPH_BUILD_ERR_ADJ_TOO_LARGE, SIZE_MAX, region_count);
            return NULL;
        }
        size_t adj_cell_bytes = adj_cell_count * sizeof(int);
        if (adj_ptr_bytes > SIZE_MAX - adj_cell_bytes) {
            free(visited);
            free(region_id);
            set_build_status(GRAPH_BUILD_ERR_ADJ_TOO_LARGE, SIZE_MAX, region_count);
            return NULL;
        }

        adj_total_bytes = adj_ptr_bytes + adj_cell_bytes;
        if (adj_total_bytes > GRAPH_ADJ_MEMORY_LIMIT_BYTES) {
            free(visited);
            free(region_id);
            set_build_status(GRAPH_BUILD_ERR_ADJ_TOO_LARGE, adj_total_bytes, region_count);
            return NULL;
        }
    }

    int** adj = NULL;
    if (region_count > 0) {
        adj = (int**)malloc((size_t)region_count * sizeof(int*));
        if (!adj) {
            free(visited);
            free(region_id);
            set_build_status(GRAPH_BUILD_ERR_MEMORY, adj_total_bytes, region_count);
            return NULL;
        }
        for (int i = 0; i < region_count; i++) {
            adj[i] = (int*)calloc((size_t)region_count, sizeof(int));
            if (!adj[i]) {
                for (int j = 0; j < i; j++) free(adj[j]);
                free(adj);
                free(visited);
                free(region_id);
                set_build_status(GRAPH_BUILD_ERR_MEMORY, adj_total_bytes, region_count);
                return NULL;
            }
        }
    }

    // второй проход: смежность только по общей границе ненулевой длины (угловые касания не считаем)
    for (int y = 0; y < h; y++) {
        int x = 0;
        while (x < w) {
            if (region_id[idx(x, y, w)] != -1) {
                x++;
                continue;
            }

            int run_start = x;
            while (x + 1 < w && region_id[idx(x + 1, y, w)] == -1) {
                x++;
            }
            int run_end = x;

            int left_id = (run_start > 0) ? region_id[idx(run_start - 1, y, w)] : -1;
            int right_id = (run_end + 1 < w) ? region_id[idx(run_end + 1, y, w)] : -1;
            mark_adjacent(adj, left_id, right_id);
            x++;
        }
    }

    for (int x = 0; x < w; x++) {
        int y = 0;
        while (y < h) {
            if (region_id[idx(x, y, w)] != -1) {
                y++;
                continue;
            }

            int run_start = y;
            while (y + 1 < h && region_id[idx(x, y + 1, w)] == -1) {
                y++;
            }
            int run_end = y;

            int top_id = (run_start > 0) ? region_id[idx(x, run_start - 1, w)] : -1;
            int bottom_id = (run_end + 1 < h) ? region_id[idx(x, run_end + 1, w)] : -1;
            mark_adjacent(adj, top_id, bottom_id);
            y++;
        }
    }

    free(visited);

    Graph* g = (Graph*)malloc(sizeof(Graph));
    if (!g) {
        if (adj) {
            for (int i = 0; i < region_count; i++) free(adj[i]);
            free(adj);
        }
        free(region_id);
        set_build_status(GRAPH_BUILD_ERR_MEMORY, adj_total_bytes, region_count);
        return NULL;
    }

    g->region_count = region_count;
    g->adj = adj;
    g->region_map = region_id;
    set_build_status(GRAPH_BUILD_OK, adj_total_bytes, region_count);
    return g;
}

void free_graph(Graph* g) {
    if (!g) return;

    for (int i = 0; i < g->region_count; i++) {
        free(g->adj[i]);
    }

    free(g->adj);
    free(g->region_map);
    free(g);
}
