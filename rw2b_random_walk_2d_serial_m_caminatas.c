#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

int main() {

    int M = 100000;   // cantidad de caminatas
    int N = 100;    // pasos por caminata

    double suma_distancias = 0.0;

    srand(time(NULL));

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

    double distancia_promedio = suma_distancias / M;

    printf("Random Walk 2D\n");
    printf("Cantidad de caminatas: %d\n", M);
    printf("Pasos por caminata: %d\n", N);
    printf("Distancia promedio: %.4f\n", distancia_promedio);

    return 0;
}

/*
gcc random_walk_2d_serial_m_caminatas.c -o random_walk_2d_serial_m_caminatas -lm 
./random_walk_2d_serial_m_caminatas
*/

