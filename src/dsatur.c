#include "dsatur.h"
#include <stdlib.h>
#include <string.h>

#define DSATUR_PRIMARY_COLORS 4
#define DSATUR_FALLBACK_COLORS 5

// считаем степень вершины v
static int get_degree(const Graph* g, int v, int n) {
    int deg = 0;
    for (int i = 0; i < n; i++) {
        if (g->adj[v][i]) deg++;
    }
    return deg;
}

// считаем насыщенность вершины v — число разных цветов у её окрашенных соседей
static int get_saturation(const Graph* g, int v, const int* colors, int n, int* used) {
    memset(used, 0, ((size_t)n + 1) * sizeof(int));

    for (int i = 0; i < n; i++) {
        if (g->adj[v][i] && colors[i] != 0) {
            used[colors[i]] = 1;
        }
    }

    int sat = 0;
    for (int i = 1; i <= n; i++) {
        if (used[i]) sat++;
    }

    return sat;
}

// выбираем следующую вершину для окраски: максимум насыщенности, при равенстве — максимум степени
static int pick_vertex(const Graph* g, const int* colors, int n, int* used) {
    int best_v = -1;
    int best_sat = -1;
    int best_deg = -1;

    for (int v = 0; v < n; v++) {
        if (colors[v] != 0) continue;

        int sat = get_saturation(g, v, colors, n, used);
        int deg = get_degree(g, v, n);
        if (sat > best_sat || (sat == best_sat && deg > best_deg)) {
            best_sat = sat;
            best_deg = deg;
            best_v = v;
        }
    }

    return best_v;
}

// проверяем, можно ли использовать цвет color для вершины v
static int can_use_color(const Graph* g, int v, int color, const int* colors, int n) {
    for (int i = 0; i < n; i++) {
        if (g->adj[v][i] && colors[i] == color) {
            return 0;
        }
    }
    return 1;
}

// рекурсивный backtracking с ограничением по максимальному номеру цвета
static int dsatur_backtrack(const Graph* g, int* colors, int n, int colored_count, int max_colors, int* used) {
    if (colored_count == n) return 1;

    int v = pick_vertex(g, colors, n, used);
    if (v == -1) return 0;

    for (int c = 1; c <= max_colors; c++) {
        if (!can_use_color(g, v, c, colors, n)) continue;

        colors[v] = c;
        if (dsatur_backtrack(g, colors, n, colored_count + 1, max_colors, used)) {
            return 1;
        }
        colors[v] = 0;
    }

    return 0;
}

int* dsatur(const Graph* g) {
    if (!g) return NULL;
    int n = g->region_count;

    if (n == 0) {
        return (int*)calloc(1, sizeof(int));
    }

    int* colors = (int*)calloc((size_t)n, sizeof(int));
    if (!colors) return NULL;

    int* used = (int*)calloc((size_t)n + 1, sizeof(int));
    if (!used) {
        free(colors);
        return NULL;
    }

    int start = 0;
    int max_deg = -1;
    for (int i = 0; i < n; i++) {
        int deg = get_degree(g, i, n);
        if (deg > max_deg) {
            max_deg = deg;
            start = i;
        }
    }

    colors[start] = 1;
    if (dsatur_backtrack(g, colors, n, 1, DSATUR_PRIMARY_COLORS, used)) {
        free(used);
        return colors;
    }

    // fallback: допускаем до 5 цветов, но выше не идём
    for (int i = 0; i < n; i++) colors[i] = 0;
    colors[start] = 1;
    if (dsatur_backtrack(g, colors, n, 1, DSATUR_FALLBACK_COLORS, used)) {
        free(used);
        return colors;
    }

    free(used);
    free(colors);
    return NULL;
}
