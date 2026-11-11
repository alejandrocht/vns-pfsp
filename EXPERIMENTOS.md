# Calibración de parámetros del VNS — Grupo G2

## 1. Idea general

Todos los experimentos usan **el mismo algoritmo: VNS**. Lo único que cambia es su configuración.

Primero se ajusta cada parámetro por separado (experimentos 1, 2 y 3) para ver qué valor funciona mejor.
Después se juntan los mejores valores en una sola configuración final y se confirma con más corridas (experimento 4).

### Cómo funciona una ronda (iteración) de VNS

1. **Shaking (empujón):** se desordena un poco la solución actual para salir del "hoyo" (óptimo local) en el que está.
   Se prueban 3 vecindarios en orden:
   - N1 **swap**: intercambiar dos trabajos (J3 ↔ J7).
   - N2 **inserción**: sacar un trabajo y ponerlo en otra posición.
   - N3 **inversión**: invertir un tramo (J3-J7-J1 → J1-J7-J3).
2. **Búsqueda local (bajada):** desde la solución desordenada, se prueban cambios pequeños y se aplica el que más baja el makespan, hasta que ya no se pueda bajar más.
3. **Decisión:** si la nueva solución es mejor, se acepta y se vuelve a N1. Si no, se pasa al siguiente vecindario.

### Parámetros que se calibraron

| Parámetro | Qué significa |
|---|---|
| `sinmejora` | Criterio de parada: cuántas rondas **seguidas** sin mejorar se aceptan antes de detenerse. Se cuenta desde la última mejora, no desde el inicio. |
| `fuerza` | Cuántos movimientos aleatorios hace cada shaking. `fuerza=2` = el empujón aplica 2 cambios al azar. |
| `bl` | Si se usa la búsqueda local (1) o no (0). Sin ella, el algoritmo se llama VNS reducido (RVNS). |

### Cómo leer las tablas

- **Corridas que llegaron al mejor:** de N corridas, cuántas encontraron el mejor makespan conocido (52, 7720 y 7312).
  Se corre varias veces porque el algoritmo usa azar; una sola corrida podría salir bien o mal por suerte.
- **Promedio y Desv.:** makespan típico y cuánto varía entre corridas. Desv. = 0 significa que siempre da lo mismo.
- **Tiempo:** milisegundos promedio por corrida. Varía un poco en cada ejecución.

El indicador principal es el **makespan** (es lo que pide minimizar el proyecto). Después importa la estabilidad y, al final, el tiempo.

Los datos se muestran para la **instancia 3 (12×5)**, la más difícil, donde más se notan las diferencias. Las tres instancias están en `experimentos/tabla_parametros.csv`.

---

## 2. Experimento 1: criterio de parada (`sinmejora`)

Se fija `fuerza=2` con búsqueda local y se cambia cuántas rondas sin mejora se esperan. 10 corridas por valor.

| sinmejora | Corridas que llegaron a 7312 | Promedio | Desv. | Tiempo |
|---|---|---|---|---|
| 1 | 3/10 | 7366.3 | 39.71 | 0.32 ms |
| 5 | 7/10 | 7334.8 | 37.79 | 0.99 ms |
| 20 | 8/10 | 7326.1 | 30.73 | 2.97 ms |
| **50** | **10/10** | **7312.0** | **0.00** | 7.00 ms |

**Conclusión:** si el algoritmo se rinde rápido, se queda atascado en soluciones peores (7399). En la curva de convergencia hay corridas que recién mejoran en la ronda 15, 22 o 26; con poco aguante se detienen antes de llegar ahí.

---

## 3. Experimento 2: fuerza del shaking (`fuerza`)

Se fija `sinmejora=5` (un valor bajo, a propósito, para que se noten las diferencias) y se cambia la fuerza. 10 corridas por valor.

| fuerza | Instancia 2: llegaron a 7720 | Instancia 3: llegaron a 7312 | Promedio inst. 3 |
|---|---|---|---|
| 1 | 4/10 | 3/10 | 7363.0 |
| 2 | 7/10 | **7/10** | **7334.8** |
| 4 | **10/10** | 6/10 | 7340.2 |

**Conclusión:** con fuerza 1 el empujón es muy suave y no saca a la solución del hoyo. Entre 2 y 4 el resultado depende de la instancia: en la 2 ayuda más la fuerza 4 y en la 3, la fuerza 2. Se eligió **fuerza 2** porque es la mejor en la instancia más difícil. Con `sinmejora=50` esta diferencia desaparece y ambas llegan siempre al mejor valor.

---

## 4. Experimento 3: ¿importa la búsqueda local?

Se compara el VNS completo contra el VNS sin búsqueda local (RVNS). 10 corridas.

| Versión | Llegaron a 7312 | Promedio | Desv. | Tiempo |
|---|---|---|---|---|
| **VNS completo** (sinmejora 50) | **10/10** | **7312.0** | **0.00** | 7.12 ms |
| RVNS (sinmejora 50) | 0/10 | 7489.7 | 138.58 | 0.05 ms |
| RVNS con 20 veces más aguante (sinmejora 1000) | 4/10 | 7360.9 | 43.27 | 0.40 ms |

**Conclusión:** la búsqueda local es la pieza más importante. Sin ella, ninguna corrida llega al mejor valor, y aun dándole muchísimas más rondas solo acierta 4 de 10.

---

## 5. Experimento 4: confirmación del criterio de parada (30 corridas)

Con la configuración final (`fuerza=2`, búsqueda local activada) se repite el barrido de `sinmejora` con **30 corridas** para que el resultado sea más confiable. Se incluye `sinmejora=100` para comprobar si esperar más ayuda.

| sinmejora | Instancia 2: llegaron a 7720 | Instancia 3: llegaron a 7312 | Tiempo inst. 3 |
|---|---|---|---|
| 5 | 23/30 | 19/30 | 1.03 ms |
| 10 | 29/30 | 23/30 | 1.79 ms |
| 15 | 30/30 | 27/30 | 2.54 ms |
| 20 | 30/30 | 28/30 | 3.22 ms |
| 25 | 30/30 | **30/30** | 4.18 ms |
| 30 | 30/30 | 30/30 | 4.45 ms |
| **50** | **30/30** | **30/30** | 7.59 ms |
| 100 | 30/30 | 30/30 | 13.14 ms |

**Conclusión:**
- **25** es el mínimo con el que siempre se llega al mejor valor en las dos instancias.
- Se eligió **50** (el doble del mínimo) como margen de seguridad.
- **100** da exactamente el mismo makespan pero tarda casi el doble. Esperar más no mejora nada cuando ya se encontró la mejor solución.

---

## 6. Configuración final

| Parámetro | Valor | Por qué |
|---|---|---|
| `sinmejora` | **50** | Desde 25 siempre llega al mejor valor; 50 da margen. 100 no mejora y tarda el doble. |
| `iter` (tope) | 200 | Límite de seguridad; nunca se alcanzó (máximo observado: 76 rondas). |
| `fuerza` | **2** | La mejor en la instancia más difícil. |
| Búsqueda local | **Sí** (VND: inserción + swap) | Sin ella no se llega al mejor valor. |
| Vecindarios | swap, inserción, inversión (kmax = 3) | — |
| Solución inicial | Aleatoria | — |
| Corridas | 10 por instancia | Lo que pide el enunciado. |

### Resultado final (10 corridas por instancia)

| Instancia | Mejor | Peor | Promedio | Desv. | Mejor secuencia | ¿Es el óptimo? |
|---|---|---|---|---|---|---|
| 1 (5×4) | 52 | 52 | 52.00 | 0.00 | J5-J2-J4-J3-J1 | Sí, comprobado por fuerza bruta |
| 2 (10×6) | 7720 | 7720 | 7720.00 | 0.00 | J4-J5-J2-J1-J3-J8-J6-J10-J9-J7 | Sí, comprobado por fuerza bruta |
| 3 (12×5) | 7312 | 7312 | 7312.00 | 0.00 | J11-J6-J12-J5-J10-J9-J3-J2-J4-J7-J8-J1 | No se puede comprobar (12! ≈ 479 millones de órdenes) |

---

## 7. Cómo reproducir

```bash
./correr_todo.sh             # configuración final: tabla, curvas de convergencia y Gantt
./experimento_parametros.sh  # experimentos 1 a 4 -> experimentos/tabla_parametros.csv
```

Las semillas son fijas (corrida r usa semilla 1 + r), así que los makespan salen iguales cada vez. Los tiempos en ms varían ligeramente según la máquina.

---

## 8. Mejora posible (para la sección 7 del informe, no implementada)

- **Solución inicial NEH:** en vez de empezar con un orden al azar, construir el orden inicial con la heurística NEH (Nawaz, Enscore y Ham, 1983), la más usada en PFSP. Ordena los trabajos de mayor a menor tiempo total y los inserta uno por uno en la mejor posición. Daría un buen punto de partida y el VNS necesitaría menos rondas para converger.
