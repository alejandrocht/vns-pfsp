#!/bin/bash
# Compila, corre las 3 instancias (10 corridas cada una) y genera los gráficos.
set -e
cd "$(dirname "$0")"
g++ -O2 -std=c++17 -Wall -o vns vns.cpp
for f in instancias/*.txt; do
    ./vns "$f" > "log_$(basename "$f" .txt).txt"
    echo "ok $f"
done
python3 graficos.py resultados
