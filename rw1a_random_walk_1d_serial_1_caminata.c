#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {

    int N = 10;  // cantidad de pasos
    int posicion = 0;
    int movimiento;

    // Inicializar el generador de números aleatorios
    srand(time(NULL));

    printf("Random Walk 1D\n");
    printf("Cantidad de pasos: %d\n\n", N);

    // Realizar la caminata
    for (int i = 1; i <= N; i++) {

        // Generar aleatoriamente -1 o +1
        movimiento = (rand() % 2 == 0) ? -1 : 1;

        // Actualizar la posición
        posicion = posicion + movimiento;

        // Mostrar información del paso
        printf("Paso %d: movimiento = %+d, posicion = %d\n",
               i, movimiento, posicion);
    }

    // Calcular distancia final al origen
    int distancia = abs(posicion);

    printf("\nPosicion final: %d\n", posicion);
    printf("Distancia final: %d\n", distancia);

    return 0;
}

/*
gcc 1_random_walk_1d_serial.c -o 1_random_walk_1d_serial
./1_random_walk_1d_serial
*/