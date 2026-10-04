#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

int main(int argc, char** argv) {
    long total_walks = 10000000; // 10 millones de caminatas
    int steps = 1000;            // Cantidad de pasos N

    // Semilla dependiente del reloj del sistema (no reproducible)
    srand(time(NULL));

    clock_t t_start = clock();

    double dist_sum = 0.0;

    // BUCLE SECUENCIAL INGENUO (BASELINE)
    for (long w = 0; w < total_walks; w++) {
        int x = 0; // Inicia en 0

        for (int s = 0; s < steps; s++) {
            // INEFICIENCIA 1: Función de sistema rand() con bloqueos internos de estado
            // INEFICIENCIA 2: Operación módulo '%' pesada para la ALU (en lugar de máscara & 1)
            int step = rand() % 2;

            // INEFICIENCIA 3: Salto condicional if/else que penaliza el predictor de saltos
            if (step == 0) {
                x = x + 1;
            } else {
                x = x - 1;
            }
        }

        // Acumulación de la distancia euclidiana |x|
        dist_sum += (double)abs(x);
    }

    clock_t t_end = clock();
    double elapsed = (double)(t_end - t_start) / CLOCKS_PER_SEC;

    double avg_dist = dist_sum / (double)total_walks;
    double theoretical = sqrt((2.0 * (double)steps) / M_PI);

    printf("================ RANDOM WALK 1D BASELINE (NO OPTIMIZADO) ================\n");
    printf("Total caminatas:           %ld\n", total_walks);
    printf("Pasos por caminata (N):    %d\n", steps);
    printf("Distancia Promedio Calc:   %.4f\n", avg_dist);
    printf("Distancia Teorica Esperada:%.4f\n", theoretical);
    printf("Tiempo secuencial (T_base):%.4f s\n", elapsed);
    printf("=========================================================================\n");

    return 0;
}
