"""Genera curvas de convergencia, diagramas de Gantt y la tabla resumen a partir de resultados/.
Los PNG quedan en la misma carpeta del proyecto: <instancia>_convergencia.png y <instancia>_gantt.png
Uso: python3 graficos.py [carpeta_resultados]
"""
import csv
import glob
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

base = sys.argv[1] if len(sys.argv) > 1 else "resultados"
OUT = os.path.dirname(os.path.abspath(__file__))
filas = []

for d in sorted(glob.glob(os.path.join(base, "*"))):
    if not os.path.isfile(os.path.join(d, "convergencia.csv")):
        continue
    nombre = os.path.basename(d)

    # Curva de convergencia: una linea por corrida, la mejor resaltada.
    with open(os.path.join(d, "convergencia.csv")) as f:
        rows = list(csv.reader(f))
    cols = list(zip(*rows[1:]))
    iters = [int(x) for x in cols[0]]
    plt.figure(figsize=(8, 4.5))
    for i, c in enumerate(cols[1:]):
        plt.plot(iters, [int(x) for x in c], alpha=0.35, lw=1, label="otras corridas" if i == 0 else None)
    mejor = min(cols[1:], key=lambda c: int(c[-1]))
    plt.plot(iters, [int(x) for x in mejor], color="red", lw=2, label="mejor corrida")
    # Acerca el eje X a donde aún hay cambios (si no, la curva queda aplastada a la izquierda).
    ultimo = max(max((k for k in range(1, len(c)) if c[k] != c[k - 1]), default=1) for c in cols[1:])
    plt.xlim(-0.5, max(10, ultimo * 2))
    plt.title(f"Curva de convergencia VNS - {nombre}")
    plt.xlabel("Iteraciones")
    plt.ylabel("Mejor makespan encontrado")
    plt.grid(alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(OUT, f"{nombre}_convergencia.png"), dpi=150)
    plt.close()

    # Gantt de la mejor secuencia.
    with open(os.path.join(d, "gantt.csv")) as f:
        ops = list(csv.DictReader(f))
    mk = max(int(o["fin"]) for o in ops)
    with open(os.path.join(d, "resumen.csv")) as f:
        resumen = next(csv.DictReader(f))

    # Verificación: el Gantt debe ser coherente con el makespan reportado y con las reglas del PFSP.
    assert mk == int(resumen["mejor"]), f"{nombre}: Gantt {mk} != resumen {resumen['mejor']}"
    sec = resumen["mejor_secuencia"].split("-")
    for m in {o["maquina"] for o in ops}:
        en_maquina = [o for o in ops if o["maquina"] == m]
        assert [o["trabajo"] for o in en_maquina] == sec, f"{nombre}: {m} no sigue la secuencia"
        for a, b in zip(en_maquina, en_maquina[1:]):   # una máquina, un trabajo a la vez
            assert int(b["inicio"]) >= int(a["fin"]), f"{nombre}: solapamiento en {m}"
    ruta_txt = os.path.join(OUT, "instancias", nombre + ".txt")
    if os.path.isfile(ruta_txt):   # duración de cada barra = tiempo del archivo de entrada
        with open(ruta_txt) as f:
            lineas = [l.split() for l in f if l.strip()]
        lineas = [l for l in lineas if all(x.isdigit() for x in l)]
        p = [dict(zip(l[0::2], l[1::2])) for l in lineas[1:]]
        for o in ops:
            esperado = int(p[int(o["trabajo"][1:]) - 1][str(int(o["maquina"][1:]) - 1)])
            assert int(o["fin"]) - int(o["inicio"]) == esperado, f"{nombre}: duración errada {o}"
    for t in sec:
        pasos = sorted((o for o in ops if o["trabajo"] == t), key=lambda o: int(o["maquina"][1:]))
        for a, b in zip(pasos, pasos[1:]):             # M1 -> M2 -> ... en orden
            assert int(b["inicio"]) >= int(a["fin"]), f"{nombre}: {t} empieza antes de terminar en {a['maquina']}"
    trabajos = sorted({o["trabajo"] for o in ops}, key=lambda j: int(j[1:]))
    cmap = plt.get_cmap("tab20")
    color = {j: cmap(i % 20) for i, j in enumerate(trabajos)}
    maqs = sorted({o["maquina"] for o in ops}, key=lambda x: int(x[1:]))
    fig, ax = plt.subplots(figsize=(11, 1 + 0.9 * len(maqs)))
    for o in ops:
        y = len(maqs) - 1 - maqs.index(o["maquina"])
        ini, fin = int(o["inicio"]), int(o["fin"])
        ax.barh(y, fin - ini, left=ini, color=color[o["trabajo"]], edgecolor="black")
        # Etiqueta con trabajo e inicio-fin; en barras angostas solo cabe el trabajo.
        ancho_rel = (fin - ini) / mk
        if ancho_rel > 0.045:
            ax.text(ini + (fin - ini) / 2, y, f"{o['trabajo']}\n{ini}-{fin}", ha="center", va="center", fontsize=7)
        elif ancho_rel > 0.015:
            ax.text(ini + (fin - ini) / 2, y, o["trabajo"], ha="center", va="center", fontsize=6)
    ax.axvline(mk, color="red", ls="--")
    ax.text(mk, -0.75, f"makespan = {mk} ", color="red", ha="right", va="center", fontweight="bold")
    ax.set_ylim(-1, len(maqs) - 0.5)
    ax.set_yticks(range(len(maqs)))
    ax.set_yticklabels(maqs[::-1])
    ax.set_xlabel("Tiempo")
    ax.set_title(f"Diagrama de Gantt - {nombre} (makespan {mk})")
    plt.tight_layout()
    plt.savefig(os.path.join(OUT, f"{nombre}_gantt.png"), dpi=150)
    plt.close()

    filas.append(resumen)
    print(f"{nombre}: Gantt verificado (makespan {mk}, sin solapamientos, precedencias correctas)")

# Tabla resumen en Markdown.
print("| Instancia | Mejor | Peor | Promedio | Desv. est. | Tiempo prom. (ms) | Mejor secuencia |")
print("|" + "---|" * 7)
for r in filas:
    print(f"| {r['instancia']} | {r['mejor']} | {r['peor']} | {float(r['promedio']):.2f} | "
          f"{float(r['desv_estandar']):.2f} | {float(r['tiempo_prom_ms']):.2f} | {r['mejor_secuencia']} |")
