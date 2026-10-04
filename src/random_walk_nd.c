#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <mpi.h>

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

static void pcg32_srandom_r(pcg32_random_t *rng, uint64_t initstate, uint64_t initseq) {
    rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    (void)pcg32_random_r(rng);
    rng->state += initstate;
    (void)pcg32_random_r(rng);
}

/*
 * Devuelve un entero uniforme en [0, bound).
 * - Si bound es potencia de 2, usa máscara binaria.
 * - En otro caso usa rejection sampling y evita el sesgo de r % bound.
 */
static inline uint32_t pcg32_bounded_r(pcg32_random_t *rng, uint32_t bound) {
    if ((bound & (bound - 1u)) == 0u) {
        return pcg32_random_r(rng) & (bound - 1u);
    }

    const uint32_t threshold = (uint32_t)(-bound) % bound;
    for (;;) {
        const uint32_t r = pcg32_random_r(rng);
        if (r >= threshold) {
            return r % bound;
        }
    }
}

static int parse_positive_int(const char *text, int *value) {
    char *end = NULL;
    const long parsed = strtol(text, &end, 10);
    if (text == end || *end != '\0' || parsed < 1 || parsed > INT_MAX) {
        return 0;
    }
    *value = (int)parsed;
    return 1;
}

static int parse_positive_long(const char *text, long *value) {
    char *end = NULL;
    const long parsed = strtol(text, &end, 10);
    if (text == end || *end != '\0' || parsed < 1) {
        return 0;
    }
    *value = parsed;
    return 1;
}

/* Aproximación asintótica por TCL multivariado + distribución Chi_D. */
static double theoretical_mean_distance(int dim, int steps) {
    const double half_d = 0.5 * (double)dim;
    const double gamma_ratio = exp(lgamma(half_d + 0.5) - lgamma(half_d));
    return sqrt((2.0 * (double)steps) / (double)dim) * gamma_ratio;
}

int main(int argc, char **argv) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int dim = 0;
    int steps = 0;
    long total_walks = 0;
    int valid_input = 1;

    if (rank == 0) {
        if (argc == 4) {
            valid_input = parse_positive_int(argv[1], &dim) &&
                          parse_positive_int(argv[2], &steps) &&
                          parse_positive_long(argv[3], &total_walks);
        } else if (argc == 1) {
            char buffer[128];

            printf("\n================ PARÁMETROS RANDOM WALK nD ================\n");

            printf("Ingrese la dimensión del espacio (D) [ej: 3]:\n> ");
            fflush(stdout);
            if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
                sscanf(buffer, "%d", &dim) != 1 || dim < 1) {
                valid_input = 0;
            }

            printf("Ingrese la cantidad de pasos (M) [ej: 1000]:\n> ");
            fflush(stdout);
            if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
                sscanf(buffer, "%d", &steps) != 1 || steps < 1) {
                valid_input = 0;
            }

            printf("Ingrese la cantidad total de caminatas (N) [ej: 10000000]:\n> ");
            fflush(stdout);
            if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
                sscanf(buffer, "%ld", &total_walks) != 1 || total_walks < 1) {
                valid_input = 0;
            }
            printf("===========================================================\n");
        } else {
            fprintf(stderr, "Uso: mpirun -np <p> ./random_walk_nd <D> <M> <N>\n");
            valid_input = 0;
        }

        if (valid_input && dim > INT_MAX / 2) {
            fprintf(stderr, "Error: D es demasiado grande para representar 2D direcciones.\n");
            valid_input = 0;
        }
    }

    MPI_Bcast(&valid_input, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (!valid_input) {
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    MPI_Bcast(&dim, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&steps, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&total_walks, 1, MPI_LONG, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Iniciando simulación con D=%d, M=%d, N=%ld en %d procesos...\n",
               dim, steps, total_walks, size);
        fflush(stdout);
    }

    const uint32_t num_dirs = (uint32_t)(2 * dim);

    long local_walks = total_walks / size;
    const long remainder = total_walks % size;
    if (rank < remainder) {
        local_walks++;
    }

    pcg32_random_t rng;
    const uint64_t base_seed = 0x853c49e6748fea9bULL;
    const uint64_t base_stream = 0xda3e39cb94b95bdbULL;
    pcg32_srandom_r(&rng, base_seed, base_stream + (uint64_t)rank);

    int *pos = (int *)malloc((size_t)dim * sizeof(*pos));
    if (pos == NULL) {
        fprintf(stderr, "Rank %d: no se pudo reservar memoria para %d coordenadas.\n", rank, dim);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    long sample_size = 0;
    int *sample_coords = NULL;
    if (rank == 0) {
        sample_size = local_walks < MAX_SAMPLE_SIZE ? local_walks : MAX_SAMPLE_SIZE;
        if (sample_size > 0) {
            sample_coords = (int *)malloc((size_t)sample_size * (size_t)dim * sizeof(*sample_coords));
            if (sample_coords == NULL) {
                fprintf(stderr, "Error: no se pudo reservar memoria para la muestra nD.\n");
                free(pos);
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    const double t_start = MPI_Wtime();

    double local_dist_sum = 0.0;

    for (long w = 0; w < local_walks; w++) {
        memset(pos, 0, (size_t)dim * sizeof(*pos));

        for (int s = 0; s < steps; s++) {
            const uint32_t dir = pcg32_bounded_r(&rng, num_dirs);
            const int axis = (int)(dir >> 1u);              /* dir / 2 */
            const int step_val = 1 - ((int)(dir & 1u) << 1); /* +1 o -1 */
            pos[axis] += step_val;
        }

        double sum_sq = 0.0;
        for (int d = 0; d < dim; d++) {
            sum_sq += (double)pos[d] * (double)pos[d];
        }
        local_dist_sum += sqrt(sum_sq);

        if (rank == 0 && w < sample_size) {
            memcpy(&sample_coords[(size_t)w * (size_t)dim],
                   pos,
                   (size_t)dim * sizeof(*pos));
        }
    }

    double global_dist_sum = 0.0;
    MPI_Reduce(&local_dist_sum, &global_dist_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    const double local_elapsed = MPI_Wtime() - t_start;
    double max_elapsed = 0.0;
    MPI_Reduce(&local_elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        const double avg_dist = global_dist_sum / (double)total_walks;
        const double theoretical_dist = theoretical_mean_distance(dim, steps);
        const double rel_error = fabs(avg_dist - theoretical_dist) / theoretical_dist * 100.0;

        printf("\n================ RESULTADOS RANDOM WALK %dD ================\n", dim);
        printf("Dimensión del espacio (D):    %d\n", dim);
        printf("Pasos por caminata (M):       %d\n", steps);
        printf("Total de caminatas (N):       %ld\n", total_walks);
        printf("Cantidad de procesos (p):     %d\n", size);
        printf("Distancia Promedio Calc:      %.6f\n", avg_dist);
        printf("Distancia Teorica Aprox. TCL: %.6f\n", theoretical_dist);
        printf("Error relativo:               %.4f %%\n", rel_error);
        printf("Tiempo kernel+reduce (max):   %.6f s\n", max_elapsed);
        printf("===========================================================\n");

        if (sample_size > 0 && sample_coords != NULL) {
            char filename[64];
            snprintf(filename, sizeof(filename), "posiciones_%dd.csv", dim);

            FILE *fp = fopen(filename, "w");
            if (fp != NULL) {
                for (int d = 0; d < dim; d++) {
                    fprintf(fp, "x%d,", d);
                }
                fprintf(fp, "distancia\n");

                for (long i = 0; i < sample_size; i++) {
                    double sq = 0.0;
                    for (int d = 0; d < dim; d++) {
                        const int val = sample_coords[(size_t)i * (size_t)dim + (size_t)d];
                        fprintf(fp, "%d,", val);
                        sq += (double)val * (double)val;
                    }
                    fprintf(fp, "%.6f\n", sqrt(sq));
                }
                fclose(fp);
                printf("-> Archivo '%s' guardado (%ld muestras).\n", filename, sample_size);
            } else {
                fprintf(stderr, "Advertencia: no se pudo crear %s.\n", filename);
            }
        }

        free(sample_coords);
    }

    free(pos);
    MPI_Finalize();
    return EXIT_SUCCESS;
}
