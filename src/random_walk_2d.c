#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <mpi.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEFAULT_WALKS 10000000L
#define DEFAULT_STEPS 1000
#define MAX_SAMPLE_SIZE 50000L

typedef struct {
    uint64_t state;
    uint64_t inc;
} pcg32_random_t;

static inline uint32_t pcg32_random_r(pcg32_random_t *rng) {
    const uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + rng->inc;
    const uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    const uint32_t rot = (uint32_t)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
}

static void pcg32_srandom_r(pcg32_random_t *rng, uint64_t initstate, uint64_t initseq) {
    rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    (void)pcg32_random_r(rng);
    rng->state += initstate;
    (void)pcg32_random_r(rng);
}

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

int main(int argc, char **argv) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int steps = DEFAULT_STEPS;
    long total_walks = DEFAULT_WALKS;
    int save_sample = 0;
    int valid_input = 1;

    if (rank == 0) {
        if (argc == 1) {
            /* Usa valores por defecto. */
        } else if (argc == 3 || argc == 4) {
            valid_input = parse_positive_int(argv[1], &steps) &&
                          parse_positive_long(argv[2], &total_walks);
            if (argc == 4) {
                save_sample = strcmp(argv[3], "--sample") == 0;
                valid_input = valid_input && save_sample;
            }
        } else {
            valid_input = 0;
        }

        if (!valid_input) {
            fprintf(stderr, "Uso: mpirun -np <p> ./random_walk_2d [M N [--sample]]\n");
        }
    }

    MPI_Bcast(&valid_input, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (!valid_input) {
        MPI_Finalize();
        return EXIT_FAILURE;
    }
    MPI_Bcast(&steps, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&total_walks, 1, MPI_LONG, 0, MPI_COMM_WORLD);
    MPI_Bcast(&save_sample, 1, MPI_INT, 0, MPI_COMM_WORLD);

    long local_walks = total_walks / size;
    const long remainder = total_walks % size;
    if (rank < remainder) local_walks++;

    pcg32_random_t rng;
    const uint64_t base_seed = 0x853c49e6748fea9bULL;
    const uint64_t base_stream = 0xda3e39cb94b95bdbULL;
    pcg32_srandom_r(&rng, base_seed, base_stream + (uint64_t)rank);

    long sample_size = 0;
    int *sample_x = NULL;
    int *sample_y = NULL;
    if (rank == 0 && save_sample) {
        sample_size = local_walks < MAX_SAMPLE_SIZE ? local_walks : MAX_SAMPLE_SIZE;
        if (sample_size > 0) {
            sample_x = (int *)malloc((size_t)sample_size * sizeof(*sample_x));
            sample_y = (int *)malloc((size_t)sample_size * sizeof(*sample_y));
            if (sample_x == NULL || sample_y == NULL) {
                free(sample_x);
                free(sample_y);
                fprintf(stderr, "Error: no se pudo reservar memoria para la muestra 2D.\n");
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    const double t_start = MPI_Wtime();

    double local_dist_sum = 0.0;

    for (long w = 0; w < local_walks; w++) {
        int x = 0;
        int y = 0;

        for (int s = 0; s < steps; s++) {
            const uint32_t dir = pcg32_random_r(&rng) & 3u;
            switch (dir) {
                case 0u: x++; break;
                case 1u: x--; break;
                case 2u: y++; break;
                default: y--; break;
            }
        }

        const double dist = sqrt((double)x * (double)x + (double)y * (double)y);
        local_dist_sum += dist;

        if (rank == 0 && save_sample && w < sample_size) {
            sample_x[w] = x;
            sample_y[w] = y;
        }
    }

    double global_dist_sum = 0.0;
    MPI_Reduce(&local_dist_sum, &global_dist_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    const double local_elapsed = MPI_Wtime() - t_start;
    double max_elapsed = 0.0;
    MPI_Reduce(&local_elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        const double avg_dist = global_dist_sum / (double)total_walks;
        const double theoretical_dist = (sqrt(M_PI) / 2.0) * sqrt((double)steps);
        const double rel_error = fabs(avg_dist - theoretical_dist) / theoretical_dist * 100.0;

        printf("================ RESULTADOS RANDOM WALK 2D ================\n");
        printf("Pasos por caminata (M):       %d\n", steps);
        printf("Total de caminatas (N):       %ld\n", total_walks);
        printf("Cantidad de procesos (p):     %d\n", size);
        printf("Distancia Promedio Calc:      %.6f\n", avg_dist);
        printf("Distancia Teorica Aprox. TCL: %.6f\n", theoretical_dist);
        printf("Error relativo:               %.4f %%\n", rel_error);
        printf("Tiempo kernel+reduce (max):   %.6f s\n", max_elapsed);
        printf("===========================================================\n");

        if (save_sample && sample_size > 0) {
            FILE *fp = fopen("posiciones_2d.csv", "w");
            if (fp != NULL) {
                fprintf(fp, "x,y,distancia\n");
                for (long i = 0; i < sample_size; i++) {
                    const double d = sqrt((double)sample_x[i] * (double)sample_x[i] +
                                          (double)sample_y[i] * (double)sample_y[i]);
                    fprintf(fp, "%d,%d,%.6f\n", sample_x[i], sample_y[i], d);
                }
                fclose(fp);
                printf("-> Archivo 'posiciones_2d.csv' guardado (%ld muestras).\n", sample_size);
            } else {
                fprintf(stderr, "Advertencia: no se pudo crear posiciones_2d.csv.\n");
            }
        }

        free(sample_x);
        free(sample_y);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
