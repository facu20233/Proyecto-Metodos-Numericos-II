#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>

int main(int argc, char *argv[]) {

    int rank, size;

    int M = 100000;
    int N = 100;

    long long caminatas_locales;

    double suma_local = 0.0;
    double suma_global = 0.0;

    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
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

    /*
     * Sincronizamos los procesos antes
     * de comenzar a medir.
     */
    MPI_Barrier(MPI_COMM_WORLD);

    double inicio = MPI_Wtime();

    /* Simulación */
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

        suma_local += distancia;
    }

    /*
     * Combinar las sumas de todos los procesos.
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
     * Sincronizamos nuevamente antes
     * de detener el cronómetro.
     */
    MPI_Barrier(MPI_COMM_WORLD);

    double fin = MPI_Wtime();

    double tiempo_local = fin - inicio;

    /*
     * Obtener el mayor tiempo entre los procesos.
     * El tiempo paralelo debe representar el tiempo
     * del proceso que termina último.
     */
    double tiempo_paralelo;

    MPI_Reduce(
        &tiempo_local,
        &tiempo_paralelo,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );

    if (rank == 0) {

        double distancia_promedio =
            suma_global / M;

        printf("Random Walk 1D - MPI\n");
        printf("Cantidad de procesos: %d\n", size);
        printf("Cantidad de caminatas: %d\n", M);
        printf("Pasos por caminata: %d\n", N);
        printf("Distancia promedio: %.4f\n",
               distancia_promedio);
        printf("Tiempo paralelo: %.6f segundos\n",
               tiempo_paralelo);
    }

    MPI_Finalize();

    return 0;
}


/*
mpicc rw1e_random_walk_1d_mpi_speedup.c -o rw1e_random_walk_1d_mpi_speedup
mpirun -np 1 ./rw1e_random_walk_1d_mpi_speedup
mpirun -np 2 ./rw1e_random_walk_1d_mpi_speedup
mpirun -np 3 ./rw1e_random_walk_1d_mpi_speedup
mpirun -np 4 ./rw1e_random_walk_1d_mpi_speedup
*/
