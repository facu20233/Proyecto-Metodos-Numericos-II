#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

int main() {

    int M = 100000;
    int N = 100;

    double suma_distancias = 0.0;

    srand(time(NULL));

    struct timespec inicio, fin;

    clock_gettime(CLOCK_MONOTONIC, &inicio);

    for (int caminata = 1; caminata <= M; caminata++) {

        int x = 0;
        int y = 0;

        for (int paso = 1; paso <= N; paso++) {

            int movimiento = rand() % 4;

            switch (movimiento) {

                case 0:
                    x = x + 1;
                    break;

                case 1:
                    x = x - 1;
                    break;

                case 2:
                    y = y + 1;
                    break;

                case 3:
                    y = y - 1;
                    break;
            }
        }

        double distancia = sqrt(x * x + y * y);

        suma_distancias = suma_distancias + distancia;
    }

    clock_gettime(CLOCK_MONOTONIC, &fin);

    double tiempo =
        (fin.tv_sec - inicio.tv_sec) +
        (fin.tv_nsec - inicio.tv_nsec) / 1e9;

    double distancia_promedio = suma_distancias / M;

    printf("Random Walk 2D Serial\n");
    printf("Cantidad de caminatas: %d\n", M);
    printf("Pasos por caminata: %d\n", N);
    printf("Distancia promedio: %.4f\n", distancia_promedio);
    printf("Tiempo serial: %.6f segundos\n", tiempo);

    return 0;
}

/*
gcc rw2b_random_walk_2d_serial_m_caminatas1_tiempo.c -o rw2b_random_walk_2d_serial_m_caminatas1_tiempo -lm
./rw2b_random_walk_2d_serial_m_caminatas1_tiempo
*/