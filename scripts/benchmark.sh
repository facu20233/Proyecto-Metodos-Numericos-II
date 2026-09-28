#!/bin/bash

# ==============================================================================
# Protocolo de Benchmarking: Comparativa Baseline vs. Optimizada/MPI
# ==============================================================================

RUNS=5
PROCS=(1 2 4 8)
OUTPUT_DIR="resultados_benchmark"
mkdir -p "$OUTPUT_DIR"

calc_median() {
    awk '{
        count[NR] = $1
    }
    END {
        for (i = 1; i <= NR; i++) {
            for (j = i + 1; j <= NR; j++) {
                if (count[i] > count[j]) {
                    tmp = count[i]
                    count[i] = count[j]
                    count[j] = tmp
                }
            }
        }
        if (NR % 2 == 1) {
            print count[int((NR + 1) / 2)]
        } else {
            print (count[NR / 2] + count[(NR / 2) + 1]) / 2.0
        }
    }'
}

echo "========================================================================"
echo " PASO 1: EVALUANDO CÓDIGOS BASELINE (SECUENCIAL SIN OPTIMIZAR -O0)"
echo "========================================================================"

gcc -O0 src/random_walk_1d_baseline.c -o random_walk_1d_baseline -lm
gcc -O0 src/random_walk_2d_baseline.c -o random_walk_2d_baseline -lm

for MODEL in "1d" "2d"; do
    BIN_BASE="random_walk_${MODEL}_baseline"
    echo -n "-> Midiendo $BIN_BASE ($RUNS corridas): "
    TMP_FILE=$(mktemp)

    for ((r=1; r<=RUNS; r++)); do
        OUT=$(./$BIN_BASE)
        # Extrae de forma limpia solo el número decimal que precede a la 's'
        T=$(echo "$OUT" | grep -i "tiempo" | grep -oE "[0-9]+\.[0-9]+" | tail -n 1)
        if [ -n "$T" ]; then
            echo "$T" >> "$TMP_FILE"
            echo -n "."
        fi
    done

    T_BASE_MEDIAN=$(calc_median < "$TMP_FILE")
    rm -f "$TMP_FILE"
    echo " Mediana: ${T_BASE_MEDIAN} s"
    echo "$T_BASE_MEDIAN" > "$OUTPUT_DIR/t_base_${MODEL}.txt"
done

echo ""
echo "========================================================================"
echo " PASO 2: EVALUANDO CÓDIGOS OPTIMIZADOS CON OPENMPI (-O3 -march=native)"
echo "========================================================================"

mpicc -O3 -march=native src/random_walk_1d.c -o random_walk_1d -lm
mpicc -O3 -march=native src/random_walk_2d.c -o random_walk_2d -lm

for MODEL in "1d" "2d"; do
    BIN="random_walk_${MODEL}"
    CSV_OUT="$OUTPUT_DIR/metricas_${BIN}.csv"
    T_BASE=$(cat "$OUTPUT_DIR/t_base_${MODEL}.txt" | tr -d '[:space:]')

    echo "procesos,tiempo_mediana,speedup_paralelo,eficiencia,speedup_global" > "$CSV_OUT"
    echo ">>> Pruebas para: $BIN (T_base = ${T_BASE} s) <<<"

    T1_MEDIAN=0

    for P in "${PROCS[@]}"; do
        echo -n "  -> $P proceso(s) [$RUNS corridas]: "
        TMP_FILE=$(mktemp)

        for ((r=1; r<=RUNS; r++)); do
            OUT=$(mpirun --oversubscribe -np "$P" ./"$BIN")
            # Extrae limpiamente solo el valor numérico flotante
            T=$(echo "$OUT" | grep -i "tiempo" | grep -oE "[0-9]+\.[0-9]+" | tail -n 1)
            if [ -n "$T" ]; then
                echo "$T" >> "$TMP_FILE"
                echo -n "."
            fi
        done

        MEDIAN_TIME=$(calc_median < "$TMP_FILE")
        rm -f "$TMP_FILE"

        if [ "$P" -eq 1 ]; then
            T1_MEDIAN="$MEDIAN_TIME"
            SP_PAR="1.0000"
            EFF="1.0000"
        else
            SP_PAR=$(echo "scale=4; $T1_MEDIAN / $MEDIAN_TIME" | bc -l)
            EFF=$(echo "scale=4; $SP_PAR / $P" | bc -l)
        fi

        # Cálculo de Speedup global respecto a baseline
        SP_GLOBAL=$(echo "scale=4; $T_BASE / $MEDIAN_TIME" | bc -l)

        echo " Mediana: ${MEDIAN_TIME}s | Sp_par: ${SP_PAR}x | Ep: ${EFF} | Sp_global: ${SP_GLOBAL}x"
        echo "$P,$MEDIAN_TIME,$SP_PAR,$EFF,$SP_GLOBAL" >> "$CSV_OUT"
    done
    echo "Resultados guardados en: $CSV_OUT"
    echo ""
done
