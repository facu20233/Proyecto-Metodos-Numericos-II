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

    /* Identificador del proceso */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* Cantidad de procesos */
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* Distribución de las caminatas */
    int base = M / size;
    int resto = M % size;

    if (rank < resto)
        caminatas_locales = base + 1;
    else
        caminatas_locales = base;

    /* Semilla diferente para cada proceso */
    srand(time(NULL) + rank);

    /* Sincronizar procesos antes de comenzar */
    MPI_Barrier(MPI_COMM_WORLD);

    /* Comenzar medición */
    double inicio = MPI_Wtime();

    /* Realizar caminatas locales */
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

    /* Reunir las sumas */
    MPI_Reduce(
        &suma_local,
        &suma_global,
        1,
        MPI_DOUBLE,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );

    /* Sincronizar antes de terminar la medición */
    MPI_Barrier(MPI_COMM_WORLD);

    /* Finalizar medición */
    double fin = MPI_Wtime();

    double tiempo_local = fin - inicio;
    double tiempo_paralelo = 0.0;

    /* Obtener el mayor tiempo */
    MPI_Reduce(
        &tiempo_local,
        &tiempo_paralelo,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );

    /* Mostrar resultados */
    if (rank == 0) {

        double distancia_promedio = suma_global / M;

        printf("Random Walk 2D MPI\n");
        printf("Cantidad de procesos: %d\n", size);
        printf("Cantidad de caminatas: %d\n", M);
        printf("Pasos por caminata: %d\n", N);
        printf("Distancia promedio: %.4f\n", distancia_promedio);
        printf("Tiempo paralelo: %.6f segundos\n", tiempo_paralelo);
    }

    MPI_Finalize();

    return 0;
}

/*
mpicc rw2ci_random_walk_2d_mp1_tiempo.c -o rw2ci_random_walk_2d_mp1_tiempo -lm
mpirun -np 4 ./rw2ci_random_walk_2d_mp1_tiempo
*/