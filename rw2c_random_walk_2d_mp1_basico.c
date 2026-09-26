#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <mpi.h>

int main(int argc, char *argv[]) {

    int rank, size;

    int M = 100000;
    int N = 100;

    int caminatas_locales;

    double suma_local = 0.0;
    double suma_global = 0.0;

    /* Inicializar MPI */
    MPI_Init(&argc, &argv);

    /* Obtener identificador del proceso */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* Obtener cantidad total de procesos */
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* Distribuir las caminatas */
    int base = M / size;
    int resto = M % size;

    if (rank < resto)
        caminatas_locales = base + 1;
    else
        caminatas_locales = base;

    /* Semilla diferente para cada proceso */
    srand(time(NULL) + rank);

    /* Realizar las caminatas locales */
    for (int caminata = 1; caminata <= caminatas_locales; caminata++) {

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

        suma_local = suma_local + distancia;
    }

    /* Reunir las sumas de todos los procesos */
    MPI_Reduce(
        &suma_local,
        &suma_global,
        1,
        MPI_DOUBLE,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );

    /* El proceso 0 calcula el promedio */
    if (rank == 0) {

        double distancia_promedio = suma_global / M;

        printf("Random Walk 2D MPI\n");
        printf("Cantidad de procesos: %d\n", size);
        printf("Cantidad de caminatas: %d\n", M);
        printf("Pasos por caminata: %d\n", N);
        printf("Distancia promedio: %.4f\n", distancia_promedio);
    }

    /* Finalizar MPI */
    MPI_Finalize();

    return 0;
}

/*
mpicc rw2c_random_walk_2d_mp1.c -o rw2c_random_walk_2d_mp1 -lm
mpirun -np 4 ./rw2c_random_walk_2d_mp1
*/