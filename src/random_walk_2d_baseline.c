#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

int main(int argc, char** argv) {
    long total_walks = 10000000; // 10M
    int steps = 1000;

    // Semilla dependiente del reloj del sistema (no reproducible)
    srand(time(NULL));

    clock_t t_start = clock();

    double dist_sum = 0.0;

    // INEFICIENCIA 1: Bucle ingenuo con operaciones de módulo '%' pesadas para la ALU
    // INEFICIENCIA 2: LLamadas a rand() estándar con saltos de control (branching)
    for (long w = 0; w < total_walks; w++) {
        int x = 0;
        int y = 0;

        for (int s = 0; s < steps; s++) {
            int dir = rand() % 4; // Operación módulo costosa en hardware

            // Ramificaciones que rompen el pipeline de la CPU
            if (dir == 0) {
                x = x + 1;
            } else if (dir == 1) {
                x = x - 1;
            } else if (dir == 2) {
                y = y + 1;
            } else if (dir == 3) {
                y = y - 1;
            }
        }

        // Evaluación de raíz por cada caminata
        dist_sum += sqrt((double)x * (double)x + (double)y * (double)y);
    }

    clock_t t_end = clock();
    double elapsed = (double)(t_end - t_start) / CLOCKS_PER_SEC;

    double avg_dist = dist_sum / (double)total_walks;

    printf("================ RANDOM WALK BASELINE (NO OPTIMIZADO) ================\n");
    printf("Total caminatas:           %ld\n", total_walks);
    printf("Pasos por caminata:        %d\n", steps);
    printf("Distancia promedio:        %.4f\n", avg_dist);
    printf("Tiempo secuencial (T_base):%.4f s\n", elapsed);
    printf("=======================================================================\n");

    return 0;
}
