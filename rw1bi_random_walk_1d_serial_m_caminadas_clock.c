#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {

    int M = 100000;
    int N = 100;

    double suma_distancias = 0.0;

    srand(time(NULL));

    clock_t inicio = clock();

    for (int caminata = 0; caminata < M; caminata++) {

        int posicion = 0;

        for (int paso = 0; paso < N; paso++) {

            int movimiento;

            if (rand() % 2 == 0)
                movimiento = -1;
            else
                movimiento = 1;

            posicion = posicion + movimiento;
        }

        int distancia = abs(posicion);

        suma_distancias += distancia;
    }

    clock_t fin = clock();

    double distancia_promedio = suma_distancias / M;

    double tiempo = (double)(fin - inicio) / CLOCKS_PER_SEC;

    printf("Random Walk 1D - Serial\n");
    printf("Cantidad de caminatas: %d\n", M);
    printf("Pasos por caminata: %d\n", N);
    printf("Distancia promedio: %.4f\n", distancia_promedio);
    printf("Tiempo serial: %.6f segundos\n", tiempo);

    return 0;
}

/*
gcc rw1bi_random_walk_1d_serial_m_caminadas_clock.c -o rw1bi_random_walk_1d_serial_m_caminadas_clock
./rw1bi_random_walk_1d_serial_m_caminadas_clock
*/