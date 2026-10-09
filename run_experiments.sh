#!/usr/bin/env bash
# Mide tiempos de random_walk para p = 1, 2, 3, ..., MAXP (TODOS los valores consecutivos).
#
# Uso:   ./run_experiments.sh [MAXP] [REPS]
#   MAXP  : maximo de procesos (default: nproc)
#   REPS  : repeticiones por configuracion (default: 10)
#
# Variables opcionales:
#   CONFIGS   "N S;N S;..."   (default abajo)
#   MPI_OPTS  opciones de mpirun (default: --bind-to core --map-by core)
# --map-by core: Le indica a MPI cómo distribuir los procesos (asigna 1 proceso por cada núcleo físico disponible). 
# --bind-to core: Le indica a MPI que ancle o fije cada proceso a un núcleo específico (afinidad de CPU). Esto evita que el sistema operativo mueva el proceso de un núcleo a otro, maximizando el uso del caché.
# Para mas de 2 procesos cambiara: MPI_OPTS:-"--oversubscribe --bind-to none"
# Salida: tiempos.csv  ->  dim,N,S,P,tiempo,dist_media,rep
#
# Nota: el bucle de repeticiones es el MAS EXTERNO, asi el ruido del sistema
# (otros usuarios, frecuencia del CPU) se reparte entre todos los p y no sesga un p en particular.

set -euo pipefail

MAXP=${1:-$(nproc)}
REPS=${2:-10}
EXE=${EXE:-./random_walk}
OUT=${OUT:-tiempos.csv}
MPI_OPTS=${MPI_OPTS:-"--oversubscribe --bind-to none"}
CONFIGS=${CONFIGS:-"100000 1000;100000 100;100000 10000"}
DIMS=${DIMS:-"1 2"}
SEED=12345

IFS=';' read -r -a CFG_ARR <<< "$CONFIGS"

echo "dim,N,S,P,tiempo,dist_media,rep" > "$OUT"

for rep in $(seq 1 "$REPS"); do
  for dim in $DIMS; do
    for cfg in "${CFG_ARR[@]}"; do
      read -r N S <<< "$cfg"
      stride=$(( S >= 1000 ? S / 1000 : 1 ))
      R=$(awk -v s="$S" 'BEGIN{printf "%d", int(4*sqrt(s))+1}')
      for p in $(seq 1 "$MAXP"); do
        # shellcheck disable=SC2086
        line=$(mpirun $MPI_OPTS -np "$p" "$EXE" "$dim" "$N" "$S" "$SEED" "$stride" "$R" 0)
        echo "${line},${rep}" >> "$OUT"
      done
    done
  done
  echo "repeticion $rep/$REPS lista" >&2
done
echo "Listo -> $OUT" >&2
