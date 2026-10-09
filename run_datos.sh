#!/usr/bin/env bash
# Genera los datos fisicos (distancia media vs pasos e histogramas) para 1D y 2D.
# Esta corrida NO se usa para medir tiempos (escribe archivos).
#
# Uso: ./run_datos.sh [P] [N]
#   P : procesos (default: nproc)
#   N : cantidad de caminantes (default: 10000000)
#
# Salida: datos/<dim>d_S<pasos>/mean_dist_<dim>d.csv y hist_<dim>d.csv

set -euo pipefail

P=${1:-$(nproc)}
N=${2:-10000000}
EXE=$(pwd)/${EXE:-random_walk}
MPI_OPTS=${MPI_OPTS:-"--oversubscribe --bind-to none"}

for dim in 1 2; do
  for S in 100 1000; do
    dir="datos/${dim}d_S${S}"
    mkdir -p "$dir"
    R=$(awk -v s="$S" 'BEGIN{printf "%d", int(4*sqrt(s))+1}')
    echo "dim=$dim S=$S N=$N p=$P" >&2
    # shellcheck disable=SC2086
    (cd "$dir" && mpirun $MPI_OPTS -np "$P" "$EXE" "$dim" "$N" "$S" 777 1 "$R" 1)
  done
done
