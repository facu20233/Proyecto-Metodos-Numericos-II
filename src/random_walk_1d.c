#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <mpi.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TOTAL_WALKS 10000000L
#define STEPS 1000
#define MAX_SAMPLE_SIZE 50000L

typedef struct {
    uint64_t state;
    uint64_t inc;
} pcg32_random_t;

static inline uint32_t pcg32_random_r(pcg32_random_t *rng) {
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + rng->inc;
    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = (uint32_t)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
}

/* Inicialización recomendada de PCG: cada rank recibe un stream distinto. */
static void pcg32_srandom_r(pcg32_random_t *rng, uint64_t initstate, uint64_t initseq) {
    rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    (void)pcg32_random_r(rng);
    rng->state += initstate;
    (void)pcg32_random_r(rng);
}

int main(int argc, char **argv) {
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const long total_walks = TOTAL_WALKS;
    const int steps = STEPS;

    /* Reparto balanceado: dos ranks difieren como máximo en una caminata. */
    long local_walks = total_walks / size;
    const long remainder = total_walks % size;
    if (rank < remainder) {
        local_walks++;
    }

    pcg32_random_t rng;
    const uint64_t base_seed = 0x853c49e6748fea9bULL;
    const uint64_t base_stream = 0xda3e39cb94b95bdbULL;
    pcg32_srandom_r(&rng, base_seed, base_stream + (uint64_t)rank);

    /* La muestra se toma fuera del resultado global; sólo sirve para validación. */
    long sample_size = 0;
    int *sample_x = NULL;
    if (rank == 0) {
        sample_size = local_walks < MAX_SAMPLE_SIZE ? local_walks : MAX_SAMPLE_SIZE;
        if (sample_size > 0) {
            sample_x = (int *)malloc((size_t)sample_size * sizeof(*sample_x));
            if (sample_x == NULL) {
                fprintf(stderr, "Error: no se pudo reservar memoria para la muestra 1D.\n");
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    const double t_start = MPI_Wtime();

    double local_dist_sum = 0.0;

    for (long w = 0; w < local_walks; w++) {
        int x = 0;

        for (int s = 0; s < steps; s++) {
            /* 0 -> +1, 1 -> -1. El compilador puede resolverlo sin branch. */
            const int step = (int)(pcg32_random_r(&rng) & 1u);
            x += 1 - (step << 1);
        }

        local_dist_sum += (double)abs(x);

        if (rank == 0 && w < sample_size) {
            sample_x[w] = x;
        }
    }

    double global_dist_sum = 0.0;
    MPI_Reduce(&local_dist_sum, &global_dist_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    const double local_elapsed = MPI_Wtime() - t_start;
    double max_elapsed = 0.0;
    MPI_Reduce(&local_elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        const double avg_dist = global_dist_sum / (double)total_walks;
        const double theoretical_dist = sqrt((2.0 * (double)steps) / M_PI);
        const double rel_error = fabs(avg_dist - theoretical_dist) / theoretical_dist * 100.0;

        printf("================ RESULTADOS RANDOM WALK 1D ================\n");
        printf("Pasos por caminata (M):       %d\n", steps);
        printf("Total de caminatas (N):       %ld\n", total_walks);
        printf("Cantidad de procesos (p):     %d\n", size);
        printf("Distancia Promedio Calc:      %.6f\n", avg_dist);
        printf("Distancia Teorica Aprox. TCL: %.6f\n", theoretical_dist);
        printf("Error relativo:               %.4f %%\n", rel_error);
        printf("Tiempo kernel+reduce (max):   %.6f s\n", max_elapsed);
        printf("===========================================================\n");

        FILE *fp = fopen("posiciones_1d.csv", "w");
        if (fp != NULL) {
            fprintf(fp, "x,distancia\n");
            for (long i = 0; i < sample_size; i++) {
                fprintf(fp, "%d,%d\n", sample_x[i], abs(sample_x[i]));
            }
            fclose(fp);
            printf("-> Archivo 'posiciones_1d.csv' guardado (%ld muestras).\n", sample_size);
        } else {
            fprintf(stderr, "Advertencia: no se pudo crear posiciones_1d.csv.\n");
        }

        free(sample_x);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
