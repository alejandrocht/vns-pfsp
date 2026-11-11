"""Junta los resumen.csv de experimentos/ en una sola tabla (CSV + Markdown).
Uso: python3 tabla_parametros.py
"""
import csv
import glob
import os
import re

def orden_natural(texto):
    # "E1_sinmejora5" antes que "E1_sinmejora20"
    return [int(x) if x.isdigit() else x for x in re.split(r"(\d+)", texto)]


filas = []
for ruta in sorted(glob.glob("experimentos/*/*/resumen.csv"), key=orden_natural):
    config = ruta.split(os.sep)[1]
    with open(ruta) as f:
        r = next(csv.DictReader(f))
    with open(ruta.replace("resumen.csv", "corridas.csv")) as f:
        corridas = list(csv.DictReader(f))
    iters = [int(c["iteraciones"]) for c in corridas]
    filas.append({**r, "config": config, "parametros": corridas[0]["parametros"],
                  "iter_prom": sum(iters) / len(iters), "makespans": [int(c["makespan"]) for c in corridas]})

# Mejor valor conocido por instancia (el mínimo encontrado en todo el experimento).
mejor_conocido = {}
for r in filas:
    mejor_conocido[r["instancia"]] = min(mejor_conocido.get(r["instancia"], 10**9), int(r["mejor"]))

with open("experimentos/tabla_parametros.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["config", "instancia", "mejor", "peor", "promedio", "desv_estandar", "tiempo_prom_ms",
                "iter_prom", "corridas_que_llegaron_al_mejor", "parametros"])
    for r in filas:
        aciertos = sum(m == mejor_conocido[r["instancia"]] for m in r["makespans"])
        r["aciertos"] = f"{aciertos}/{len(r['makespans'])}"
        w.writerow([r["config"], r["instancia"], r["mejor"], r["peor"], f"{float(r['promedio']):.1f}",
                    f"{float(r['desv_estandar']):.2f}", f"{float(r['tiempo_prom_ms']):.3f}",
                    f"{r['iter_prom']:.1f}", r["aciertos"], r["parametros"]])

for inst in sorted(mejor_conocido):
    print(f"\n### {inst} (mejor conocido: {mejor_conocido[inst]})")
    print("| Config | Mejor | Peor | Promedio | Desv. | Tiempo (ms) | Iter. prom. | Corridas que llegaron al mejor |")
    print("|---|---|---|---|---|---|---|---|")
    for r in filas:
        if r["instancia"] == inst:
            print(f"| {r['config']} | {r['mejor']} | {r['peor']} | {float(r['promedio']):.1f} | "
                  f"{float(r['desv_estandar']):.2f} | {float(r['tiempo_prom_ms']):.3f} | {r['iter_prom']:.1f} | {r['aciertos']} |")
