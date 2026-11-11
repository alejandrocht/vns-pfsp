#!/bin/bash
# Experimento de sensibilidad de parámetros (sección 6 del informe).
# Cada configuración corre 10 veces por instancia; los CSV quedan en experimentos/<config>/<instancia>/.
set -e
cd "$(dirname "$0")"
g++ -O2 -std=c++17 -Wall -o vns vns.cpp
rm -rf experimentos

# nombre | opciones
CONFIGS=(
    "E1_sinmejora1|--bl 1 --sinmejora 1 --fuerza 2"
    "E1_sinmejora5|--bl 1 --sinmejora 5 --fuerza 2"
    "E1_sinmejora20|--bl 1 --sinmejora 20 --fuerza 2"
    "E1_sinmejora50|--bl 1 --sinmejora 50 --fuerza 2"
    "E2_fuerza1|--bl 1 --sinmejora 5 --fuerza 1"
    "E2_fuerza2|--bl 1 --sinmejora 5 --fuerza 2"
    "E2_fuerza4|--bl 1 --sinmejora 5 --fuerza 4"
    "E3_VNS|--bl 1 --sinmejora 50 --fuerza 2"
    "E3_RVNS|--bl 0 --sinmejora 50 --fuerza 2"
    "E3_RVNS_largo|--bl 0 --sinmejora 1000 --iter 5000 --fuerza 2"
)
# E4: barrido fino del criterio de parada con 30 corridas (tope alto para que no corte antes).
for sm in 5 10 15 20 25 30 50 100; do
    CONFIGS+=("E4_sinmejora${sm}|--corridas 30 --bl 1 --sinmejora $sm --iter 1000 --fuerza 2")
done

for c in "${CONFIGS[@]}"; do
    nombre="${c%%|*}"
    opciones="${c#*|}"
    for f in instancias/*.txt; do
        ./vns "$f" --corridas 10 $opciones --salida "experimentos/$nombre" > /dev/null
    done
    echo "ok $nombre"
done
rm -f vns
python3 tabla_parametros.py
