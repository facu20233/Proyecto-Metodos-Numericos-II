#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEFAULT_WALKS 10000000L
#define DEFAULT_STEPS 1000

static int parse_positive_int(const char *text, int *value) {
    char *end = NULL;
    const long parsed = strtol(text, &end, 10);
    if (text == end || *end != '\0' || parsed < 1 || parsed > INT_MAX) return 0;
    *value = (int)parsed;
    return 1;
}

static int parse_positive_long(const char *text, long *value) {
    char *end = NULL;
    const long parsed = strtol(text, &end, 10);
    if (text == end || *end != '\0' || parsed < 1) return 0;
    *value = parsed;
    return 1;
}

static double wall_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
    int steps = DEFAULT_STEPS;
    long total_walks = DEFAULT_WALKS;

    if (argc == 3) {
        if (!parse_positive_int(argv[1], &steps) || !parse_positive_long(argv[2], &total_walks)) {
            fprintf(stderr, "Uso: ./random_walk_1d_baseline [M N]\n");
            return EXIT_FAILURE;
        }
    } else if (argc != 1) {
        fprintf(stderr, "Uso: ./random_walk_1d_baseline [M N]\n");
        return EXIT_FAILURE;
    }

    /* Baseline deliberadamente simple y reproducible. */
    srand(123456789u);
    const double t_start = wall_time();

    double dist_sum = 0.0;
    for (long w = 0; w < total_walks; w++) {
        int x = 0;
        for (int s = 0; s < steps; s++) {
            const int step = rand() % 2;
            if (step == 0) x = x + 1;
            else x = x - 1;
        }
        dist_sum += (double)abs(x);
    }

    const double elapsed = wall_time() - t_start;
    const double avg_dist = dist_sum / (double)total_walks;
    const double theoretical = sqrt((2.0 * (double)steps) / M_PI);

    printf("================ RANDOM WALK 1D BASELINE (-O0) ================\n");
    printf("Total caminatas (N):       %ld\n", total_walks);
    printf("Pasos por caminata (M):    %d\n", steps);
    printf("Distancia Promedio Calc:   %.6f\n", avg_dist);
    printf("Distancia Teorica Aprox.:  %.6f\n", theoretical);
    printf("Tiempo secuencial (T_base): %.6f s\n", elapsed);
    printf("===============================================================\n");
    return EXIT_SUCCESS;
}
