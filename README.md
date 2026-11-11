# VNS para PFSP (Grupo G2)

## Uso rápido
```bash
./correr_todo.sh     # compila, corre las 3 instancias x 10 corridas y genera los gráficos
```

Manual:
```bash
g++ -O2 -std=c++17 -o vns vns.cpp
./vns instancias/instancia2_car5.txt --corridas 10 --sinmejora 50 --iter 200 --fuerza 2 --semilla 1
python3 graficos.py resultados     # requiere matplotlib
```

## Archivos
- `vns.cpp`: algoritmo VNS. Escribe los CSV en `resultados/<instancia>/`.
  - `corridas.csv`: semilla, makespan, tiempo, iteraciones, secuencia y parámetros de cada corrida.
  - `convergencia.csv`: mejor makespan por iteración, una columna por corrida.
  - `gantt.csv`: máquina, trabajo, inicio y fin de la mejor secuencia.
  - `resumen.csv`: mejor, peor, promedio, desviación estándar, tiempo promedio, mejor secuencia y óptimo exacto (si n <= 10).
- `graficos.py`: genera `<instancia>_convergencia.png` y `<instancia>_gantt.png` en esta carpeta.
- `log_<instancia>.txt`: salida de consola de cada instancia.
- `experimento_parametros.sh`: corre 10 configuraciones x 3 instancias x 10 corridas en `experimentos/`
  y llama a `tabla_parametros.py`, que genera `experimentos/tabla_parametros.csv`.
  - E1: criterio de parada (`--sinmejora` 1, 5, 20, 50).
  - E2: fuerza del shaking (`--fuerza` 1, 2, 4) con `--sinmejora 5`.
  - E3: VNS completo contra VNS reducido sin búsqueda local (`--bl 0`).

## Algoritmo
- Solución: permutación de trabajos. Solución inicial aleatoria más búsqueda local.
- Shaking con 3 vecindarios: N1 swap, N2 inserción, N3 inversión (kmax = 3).
- Búsqueda local VND: la mejor inserción y, cuando ya no mejora, el mejor swap.
- Si mejora, vuelve a N1; si no, pasa a N(k+1).
- Parada: 50 iteraciones seguidas sin mejora, con un tope de 200.
