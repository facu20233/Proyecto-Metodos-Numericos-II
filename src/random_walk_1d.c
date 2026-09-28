#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <mpi.h>

// Definición de estructura para el generador pseudoaleatorio PCG32.
// Almacena el estado interno (state) y el identificador de flujo (inc) de 64 bits.
typedef struct { 
    uint64_t state; 
    uint64_t inc; 
} pcg32_random_t;

// Función generadora de números aleatorios uniforme por proceso.
// Se define static inline para que el compilador la inserte en el bucle
// sin sobrecarga de llamada a función, operando solo en registros de CPU.
static inline uint32_t pcg32_random_r(pcg32_random_t* rng) {
    uint64_t oldstate = rng->state;
    // Multiplicador congruencial lineal estándar de 64 bits
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
    // Permutación xorshift para eliminar correlación en bits bajos
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

int main(int argc, char** argv) {
    int rank, size;

    // 1. Inicialización del entorno de paso de mensajes de MPI
    MPI_Init(&argc, &argv);

    // 2. Obtener el ID/rango del proceso actual (0 a size-1)
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // 3. Obtener el número total de procesos en ejecución
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Parámetros de la simulación Monte Carlo
    long total_walks = 10000000; // 10 millones de caminatas totales
    int steps = 1000;            // Cantidad de pasos por caminata (N)

    // Descomposición de datos (Data Set Partition):
    // Se divide el total de caminatas de forma equitativa entre los procesos.
    long local_walks = total_walks / size;
    long remainder = total_walks % size;

    // Balance de carga (Load Balancing): si la división no es exacta,
    // se reparte el sobrante (+1 caminata) a los primeros procesos para evitar ocio.
    if (rank < remainder) {
        local_walks++;
    }

    // Inicialización del generador PRNG desacoplado por proceso:
    // Vinculamos la semilla y el flujo (inc) al 'rank' para asegurar que
    // ningún proceso repita la misma secuencia ni simule los mismos caminos.
    pcg32_random_t rng;
    rng.state = 0x853c49e6748fea9bULL + (uint64_t)rank * 0xda3e39cb94b95bdbULL;
    rng.inc = ((uint64_t)rank << 1u) | 1u;

    // Sincronización previa colectiva:
    // Garantiza que todos los procesos arranquen juntos antes de medir el tiempo.
    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    double local_dist_sum = 0.0;

    // Muestra para el histograma de frecuencias:
    // Solo el proceso 0 reserva memoria RAM para guardar 50.000 posiciones finales.
    // Esto ahorra memoria en los demás procesos y evita cuellos de botella de disco.
    int sample_size = (rank == 0) ? 50000 : 0;
    int *sample_x = NULL;
    if (rank == 0) {
        sample_x = (int *)malloc(sample_size * sizeof(int));
    }

    // --- BUCLE PRINCIPAL DE CÓMPUTO MONTE CARLO ---
    for (long w = 0; w < local_walks; w++) {
        int x = 0; // Cada caminata inicia obligatoriamente en el origen x = 0

        for (int s = 0; s < steps; s++) {
            // Operación a nivel de bits: & 1 extrae el bit menos significativo (0 o 1).
            // Ambas opciones tienen exactamente 50% de probabilidad.
            uint32_t step = pcg32_random_r(&rng) & 1;
            if (step == 0) {
                x++; // Desplazamiento a la derecha (+1)
            } else {
                x--; // Desplazamiento a la izquierda (-1)
            }
        }

        // En 1D, la distancia euclidiana al origen es el valor absoluto |x|.
        // Se acumula la distancia en una variable local de doble precisión.
        local_dist_sum += (double)abs(x);

        // Guardado de la muestra en el proceso 0 para exportar el histograma
        if (rank == 0 && w < sample_size) {
            sample_x[w] = x;
        }
    }

    // Reducción colectiva de la suma de distancias:
    // Suma los valores de local_dist_sum de todos los procesos y guarda
    // el consolidado final en global_dist_sum únicamente en el proceso 0.
    double global_dist_sum = 0.0;
    MPI_Reduce(&local_dist_sum, &global_dist_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    // Medición del tiempo paralelo de ejecución (Tp):
    // Se toma el tiempo que le tomó al proceso local finalizar.
    double t_end = MPI_Wtime();
    double local_elapsed = t_end - t_start;
    double max_elapsed = 0.0;

    // Con MPI_MAX determinamos el tiempo del proceso más lento (tiempo real del cluster).
    MPI_Reduce(&local_elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // --- SECCIÓN SERIAL FINAL (Únicamente ejecutada por rank 0) ---
    if (rank == 0) {
        // Cálculo del promedio experimental
        double avg_dist = global_dist_sum / (double)total_walks;

        // Solución analítica teórica en 1D: sqrt(2 * N / pi)
        double theoretical_dist = sqrt((2.0 * (double)steps) / M_PI);

        printf("================ RESULTADOS RANDOM WALK 1D ================\n");
        printf("Pasos por caminata (N):    %d\n", steps);
        printf("Total de caminatas (W):    %ld\n", total_walks);
        printf("Cantidad de procesos (p):  %d\n", size);
        printf("Distancia Promedio Calc:   %.4f\n", avg_dist);
        printf("Distancia Teorica Esperada:%.4f\n", theoretical_dist);
        printf("Tiempo de ejecucion (max): %.6f s\n", max_elapsed);
        printf("===========================================================\n");

        // Escritura de la muestra en formato CSV para graficar el histograma
        FILE *fp = fopen("posiciones_1d.csv", "w");
        if (fp != NULL) {
            fprintf(fp, "x,distancia\n");
            for (int i = 0; i < sample_size; i++) {
                fprintf(fp, "%d,%d\n", sample_x[i], abs(sample_x[i]));
            }
            fclose(fp);
            printf("Archivo 'posiciones_1d.csv' exportado con %d muestras.\n", sample_size);
        }

        // Liberación de la memoria dinámica
        free(sample_x);
    }

    // 4. Finalización formal del entorno MPI
    MPI_Finalize();
    return 0;
}
