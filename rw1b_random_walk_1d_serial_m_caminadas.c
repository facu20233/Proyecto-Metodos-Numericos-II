#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {

    int M = 100000;  // Cantidad de caminatas
    int N = 100;   // Cantidad de pasos por caminata

    long long suma_distancias = 0;

    // Inicializar generador aleatorio
    srand(time(NULL));

    // Realizar M caminatas
    for (int caminata = 1; caminata <= M; caminata++) {

        int posicion = 0;

        // Realizar N pasos
        for (int paso = 1; paso <= N; paso++) {

            int movimiento;

            // Generar -1 o +1
            movimiento = (rand() % 2 == 0) ? -1 : 1;

            // Actualizar posición
            posicion = posicion + movimiento;
        }

        // Distancia final de esta caminata
        int distancia = abs(posicion);

        // Acumular distancia
        suma_distancias += distancia;
    }

    // Calcular distancia promedio
    double distancia_promedio =
        (double)suma_distancias / M;

    printf("Random Walk 1D\n");
    printf("Cantidad de caminatas: %d\n", M);
    printf("Pasos por caminata: %d\n", N);
    printf("Distancia promedio: %.4f\n",
           distancia_promedio);

    return 0;
}

/*
gcc rw1b_random_walk_1d_serial_m_caminadas.c -o rw1b_random_walk_1d_serial_m_caminadas
./rw1b_random_walk_1d_serial_m_caminadas
*/