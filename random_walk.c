/*
 * Caminata del borracho 1D / 2D con MPI.
 *
 * Compilar:  mpicc -O3 -o random_walk random_walk.c -lm
 * Ejecutar:  mpirun -np P ./random_walk dim N S [seed] [stride] [R] [write]
 *
 *   dim    : 1 o 2
 *   N      : cantidad TOTAL de caminantes (se reparten entre los procesos)
 *   S      : cantidad de pasos de cada caminata
 *   seed   : semilla base (default 12345)
 *   stride : cada cuantos pasos se registra la distancia (default 1)
 *   R      : radio del histograma, se cuentan posiciones en [-R, R]
 *            (default ceil(4*sqrt(S)), acotado a S)
 *   write  : 1 = escribe CSVs (default 0, util para medir tiempos)
 *
 * Salida por stdout (solo rank 0), una linea CSV:
 *   dim,N,S,P,tiempo_max_seg,dist_final_media
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

/* ---------- Generador xoshiro256**  ---------- */
typedef struct { uint64_t s[4]; } rng_t;

static inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

static uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static void rng_seed(rng_t *r, uint64_t seed) {
    for (int i = 0; i < 4; i++) r->s[i] = splitmix64(&seed);
}

static inline uint64_t rng_next(rng_t *r) {
    uint64_t *s = r->s;
    uint64_t result = rotl(s[1] * 5, 7) * 9;
    uint64_t t = s[1] << 17;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 45);
    return result;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 4) {
        if (rank == 0)
            fprintf(stderr, "Uso: %s dim N S [seed] [stride] [R] [write]\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    int dim          = atoi(argv[1]);
    long long N      = atoll(argv[2]);
    long S           = atol(argv[3]);
    uint64_t seed    = (argc > 4) ? strtoull(argv[4], NULL, 10) : 12345ULL;
    long stride      = (argc > 5) ? atol(argv[5]) : 1;
    long R           = (argc > 6) ? atol(argv[6]) : (long)ceil(4.0 * sqrt((double)S));
    int write_out    = (argc > 7) ? atoi(argv[7]) : 0;

    if ((dim != 1 && dim != 2) || N < 1 || S < 1 || stride < 1) {
        if (rank == 0) fprintf(stderr, "Parametros invalidos\n");
        MPI_Finalize();
        return 1;
    }
    if (R > S) R = S;
    if (R < 1) R = 1;

    /* Reparto de caminantes: N/P para cada uno, y el resto a los primeros */
    long long local_n = N / size + (rank < N % size ? 1 : 0);

    long nrec = S / stride + 1;                 /* puntos de registro: t = 0, stride, 2*stride... */
    long W = 2 * R + 1;                         /* ancho del histograma */
    long long hist_size = (dim == 1) ? W : W * W;

    double    *sum_d   = calloc(nrec, sizeof(double));
    long long *hist    = calloc(hist_size, sizeof(long long));
    double    *g_sum_d = NULL;
    long long *g_hist  = NULL;
    if (rank == 0) {
        g_sum_d = calloc(nrec, sizeof(double));
        g_hist  = calloc(hist_size, sizeof(long long));
    }
    if (!sum_d || !hist || (rank == 0 && (!g_sum_d || !g_hist))) {
        fprintf(stderr, "Rank %d: sin memoria (reduci R o S)\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* Semilla distinta por proceso */
    rng_t rng;
    rng_seed(&rng, seed + 0x9E3779B97F4A7C15ULL * (uint64_t)(rank + 1));

    double local_final_sum = 0.0;

    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();

    /* ------------------------- simulacion local ------------------------- */
    for (long long w = 0; w < local_n; w++) {
        long x = 0, y = 0;
        /* t = 0 -> distancia 0, no hace falta sumar nada */
        for (long t = 1; t <= S; t++) {
            uint64_t r = rng_next(&rng);
            if (dim == 1) {
                x += (r >> 63) ? 1 : -1;
            } else {
                switch (r >> 62) {
                    case 0:  x++; break;
                    case 1:  x--; break;
                    case 2:  y++; break;
                    default: y--; break;
                }
            }
            if (t % stride == 0) {
                double d = (dim == 1) ? (double)labs(x)
                                      : sqrt((double)(x * x + y * y));
                sum_d[t / stride] += d;
            }
        }
        /* posicion final -> histograma (se ignoran las que caen fuera de [-R,R]) */
        if (dim == 1) {
            if (labs(x) <= R) hist[x + R]++;
            local_final_sum += (double)labs(x);
        } else {
            if (labs(x) <= R && labs(y) <= R) hist[(long long)(y + R) * W + (x + R)]++;
            local_final_sum += sqrt((double)(x * x + y * y));
        }
    }

    /* ------------------------------ reduccion ----------------------------- */
    double global_final_sum = 0.0;
    MPI_Reduce(sum_d, g_sum_d, (int)nrec, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(hist, g_hist, (int)hist_size, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_final_sum, &global_final_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    double t1 = MPI_Wtime();
    double local_time = t1 - t0, max_time;
    MPI_Reduce(&local_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    /* ------------------------------- salida ------------------------------- */
    if (rank == 0) {
        printf("%d,%lld,%ld,%d,%.6f,%.6f\n",
               dim, N, S, size, max_time, global_final_sum / (double)N);

        if (write_out) {
            char fname[64];

            /* distancia media en funcion de los pasos */
            snprintf(fname, sizeof fname, "mean_dist_%dd.csv", dim);
            FILE *f = fopen(fname, "w");
            fprintf(f, "paso,dist_media\n");
            for (long k = 0; k < nrec; k++)
                fprintf(f, "%ld,%.6f\n", k * stride, g_sum_d[k] / (double)N);
            fclose(f);

            /* histograma de la posicion final (solo celdas con cuentas) */
            snprintf(fname, sizeof fname, "hist_%dd.csv", dim);
            f = fopen(fname, "w");
            if (dim == 1) {
                fprintf(f, "x,cuentas,prob\n");
                for (long i = 0; i < W; i++)
                    if (g_hist[i])
                        fprintf(f, "%ld,%lld,%.8f\n", i - R, g_hist[i], (double)g_hist[i] / N);
            } else {
                fprintf(f, "x,y,cuentas,prob\n");
                for (long j = 0; j < W; j++)
                    for (long i = 0; i < W; i++) {
                        long long c = g_hist[j * W + i];
                        if (c) fprintf(f, "%ld,%ld,%lld,%.8f\n", i - R, j - R, c, (double)c / N);
                    }
            }
            fclose(f);
        }
    }

    free(sum_d); free(hist); free(g_sum_d); free(g_hist);
    MPI_Finalize();
    return 0;
}
