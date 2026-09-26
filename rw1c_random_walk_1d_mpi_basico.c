#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>

int main(int argc, char *argv[]) {

    int rank, size;

    int M = 100000;  // cantidad total de caminatas
    int N = 100;     // pasos por caminata

    long long caminatas_locales;
    double suma_local = 0.0;
    double suma_global = 0.0;

    /* Inicializar MPI */
    MPI_Init(&argc, &argv);

    /* Obtener identificador del proceso */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* Obtener cantidad total de procesos */
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /*
     * Para esta primera versión suponemos
     * que M es divisible por el número de procesos.
     */
    caminatas_locales = M / size;

    /* Semilla diferente para cada proceso */
    srand(time(NULL) + rank);

    /*
     * Cada proceso realiza sus caminatas locales
     */
    for (long long caminata = 0;
         caminata < caminatas_locales;
         caminata++) {

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

        suma_local = suma_local + distancia;
    }

    /*
     * Sumar las contribuciones de todos
     * los procesos.
     */
    MPI_Reduce(
        &suma_local,
        &suma_global,
        1,
        MPI_DOUBLE,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );

    /*
     * Solamente el proceso 0 muestra
     * el resultado global.
     */
    if (rank == 0) {

        double distancia_promedio;

        distancia_promedio = suma_global / M;

        printf("Random Walk 1D - MPI\n");
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
mpicc rw1c_random_walk_1d_mpi.c -o rw1c_random_walk_1d_mpi
mpirun -np 4 ./rw1c_random_walk_1d_mpi
*/