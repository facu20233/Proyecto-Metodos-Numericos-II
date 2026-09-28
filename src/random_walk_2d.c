#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <mpi.h>

// Definición de estructura para el generador pseudoaleatorio PCG32
typedef struct { 
    uint64_t state; 
    uint64_t inc; 
} pcg32_random_t;

// PRNG desacoplado y rápido por proceso en registros de CPU
static inline uint32_t pcg32_random_r(pcg32_random_t* rng) {
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

int main(int argc, char** argv) {
    int rank, size;

    // 1. Inicialización de MPI
    MPI_Init(&argc, &argv);

    // 2. Identificación del proceso y total de procesos
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Parámetros del experimento
    long total_walks = 10000000; // 10 millones de caminatas
    int steps = 1000;            // Cantidad de pasos por caminata (N)

    // Descomposición de datos y balance de carga
    long local_walks = total_walks / size;
    long remainder = total_walks % size;
    if (rank < remainder) {
        local_walks++;
    }

    // Inicialización independiente del PRNG por proceso
    pcg32_random_t rng;
    rng.state = 0x853c49e6748fea9bULL + (uint64_t)rank * 0xda3e39cb94b95bdbULL;
    rng.inc = ((uint64_t)rank << 1u) | 1u;

    // Sincronización y toma de tiempo
    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    double local_dist_sum = 0.0;

    // Arreglos dinámicos para almacenar la muestra del histograma en 2D
    int sample_size = (rank == 0) ? 50000 : 0;
    int *sample_x = NULL;
    int *sample_y = NULL;
    if (rank == 0) {
        sample_x = (int *)malloc(sample_size * sizeof(int));
        sample_y = (int *)malloc(sample_size * sizeof(int));
    }

    // --- BUCLE PRINCIPAL DE CÓMPUTO MONTE CARLO 2D ---
    for (long w = 0; w < local_walks; w++) {
        int x = 0; // Coordenada X inicial
        int y = 0; // Coordenada Y inicial

        for (int s = 0; s < steps; s++) {
            // Operación a nivel de bits: & 3 calcula el módulo 4 (valores 0, 1, 2 o 3).
            // Representa las 4 direcciones ortogonales equiprobables (25% cada una).
            uint32_t dir = pcg32_random_r(&rng) & 3;

            if (dir == 0) {
                x++; // Movimiento hacia +X (Derecha)
            } else if (dir == 1) {
                x--; // Movimiento hacia -X (Izquierda)
            } else if (dir == 2) {
                y++; // Movimiento hacia +Y (Arriba)
            } else {
                y--; // Movimiento hacia -Y (Abajo)
            }
        }

        // Distancia euclidiana final desde el origen (0,0) mediante Teorema de Pitágoras
        double dist = sqrt((double)x * (double)x + (double)y * (double)y);
        local_dist_sum += dist;

        // Registro de coordenadas en el proceso raíz para el histograma
        if (rank == 0 && w < sample_size) {
            sample_x[w] = x;
            sample_y[w] = y;
        }
    }

    // Reducción colectiva de distancias consolidadas hacia el proceso 0
    double global_dist_sum = 0.0;
    MPI_Reduce(&local_dist_sum, &global_dist_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    // Medición del tiempo paralelo de ejecución
    double t_end = MPI_Wtime();
    double local_elapsed = t_end - t_start;
    double max_elapsed = 0.0;

    // Reducción colectiva del tiempo máximo
    MPI_Reduce(&local_elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // --- SECCIÓN SERIAL FINAL (rank 0) ---
    if (rank == 0) {
        double avg_dist = global_dist_sum / (double)total_walks;

        // Distancia teórica esperada en 2D: (sqrt(pi) / 2) * sqrt(N)
        double theoretical_dist = 0.886226925 * sqrt((double)steps);

        printf("================ RESULTADOS RANDOM WALK 2D ================\n");
        printf("Pasos por caminata (N):    %d\n", steps);
        printf("Total de caminatas (W):    %ld\n", total_walks);
        printf("Cantidad de procesos (p):  %d\n", size);
        printf("Distancia Promedio Calc:   %.4f\n", avg_dist);
        printf("Distancia Teorica Esperada:%.4f\n", theoretical_dist);
        printf("Tiempo de ejecucion (max): %.6f s\n", max_elapsed);
        printf("===========================================================\n");

        // Exportación de la muestra a CSV (x, y, distancia)
        FILE *fp = fopen("posiciones_2d.csv", "w");
        if (fp != NULL) {
            fprintf(fp, "x,y,distancia\n");
            for (int i = 0; i < sample_size; i++) {
                double d = sqrt((double)sample_x[i] * sample_x[i] + (double)sample_y[i] * sample_y[i]);
                fprintf(fp, "%d,%d,%.4f\n", sample_x[i], sample_y[i], d);
            }
            fclose(fp);
            printf("Archivo 'posiciones_2d.csv' exportado con %d muestras.\n", sample_size);
        }

        // Liberación de memoria
        free(sample_x);
        free(sample_y);
    }

    // 4. Finalización formal del entorno MPI
    MPI_Finalize();
    return 0;
}
