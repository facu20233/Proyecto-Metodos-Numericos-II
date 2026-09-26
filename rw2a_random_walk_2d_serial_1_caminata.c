#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

int main() {

    int N = 10;

    int x = 0;
    int y = 0;

    int movimiento;

    // Inicializar el generador de números aleatorios
    srand(time(NULL));

    printf("Random Walk 2D\n");
    printf("Cantidad de pasos: %d\n\n", N);

    // Realizar la caminata
    for (int i = 1; i <= N; i++) {

        // Generar un número entre 0 y 3
        movimiento = rand() % 4;

        switch (movimiento) {

            case 0:
                x = x + 1;
                printf("Paso %d: derecha   ", i);
                break;

            case 1:
                x = x - 1;
                printf("Paso %d: izquierda ", i);
                break;

            case 2:
                y = y + 1;
                printf("Paso %d: arriba    ", i);
                break;

            case 3:
                y = y - 1;
                printf("Paso %d: abajo     ", i);
                break;
        }

        printf("posicion = (%d, %d)\n", x, y);
    }

    // Calcular distancia al origen
    double distancia = sqrt(x * x + y * y);

    printf("\nPosicion final: (%d, %d)\n", x, y);
    printf("Distancia final: %.4f\n", distancia);

    return 0;
}

/*
gcc 3_random_walk_2d_serial.c -o 3_random_walk_2d_serial -lm
./3_random_walk_2d_serial
*/