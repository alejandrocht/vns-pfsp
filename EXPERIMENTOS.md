# Calibración de parámetros del VNS — Grupo G2

## 0. El problema y el objetivo

### El problema (PFSP)
Una empresa tiene varios **trabajos** que deben pasar por varias **máquinas**, siempre en el mismo orden (M1 → M2 → … → Mm).
Cada trabajo tarda un tiempo distinto en cada máquina. Lo único que se puede decidir es **en qué orden entran los trabajos**,
y ese mismo orden se usa en todas las máquinas.

Hay dos reglas:
1. Un trabajo no puede empezar en una máquina hasta terminar en la anterior.
2. Una máquina procesa un solo trabajo a la vez.

### ¿Qué es el makespan?
Es **el momento en que termina el último trabajo en la última máquina**, es decir, cuánto tarda todo el proceso.
Se calcula con la tabla de tiempos de finalización:

```
C[i][j] = max(C[i-1][j], C[i][j-1]) + p[i][j]
```

- `C[i-1][j]`: cuándo se libera la máquina j (terminó el trabajo anterior).
- `C[i][j-1]`: cuándo terminó este trabajo en la máquina anterior.
- `p[i][j]`: lo que tarda este trabajo en esta máquina.

El makespan es la última celda de la tabla. **Un orden distinto da un makespan distinto.**

Ejemplo con la instancia 1: el orden J1-J2-J3-J4-J5 da un makespan de 57 y el orden J5-J2-J4-J3-J1 da 52.

### Objetivo del algoritmo
**Encontrar el orden de los trabajos con el menor makespan posible.**

¿Por qué no probar todos los órdenes? Porque crecen muy rápido:

| Trabajos | Órdenes posibles |
|---|---|
| 5 | 120 |
| 10 | 3 628 800 |
| 12 | 479 001 600 |

Por eso se usa una metaheurística (VNS), que busca un orden muy bueno sin revisar todos.

### Objetivo de la calibración
VNS tiene parámetros que se eligen antes de ejecutarlo. Los experimentos de este documento sirven para
**elegir los valores con los que VNS encuentra el menor makespan de forma confiable** y justificar esa elección con datos.

---

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

### ¿Qué es la fuerza?
Es **cuántos cambios al azar hace cada empujón (shaking)** antes de la búsqueda local.
El tipo de cambio lo decide el vecindario de ese momento (swap, inserción o inversión).

Ejemplo con swap, partiendo de J1-J2-J3-J4-J5:
- **fuerza 1:** un intercambio → J1-**J4**-J3-**J2**-J5
- **fuerza 2:** dos intercambios seguidos → J1-J4-J3-J2-J5 → **J5**-J4-J3-J2-**J1**
- **fuerza 4:** cuatro intercambios seguidos → el orden queda mucho más revuelto.

Si el empujón es muy suave, la búsqueda local vuelve al mismo hoyo. Si es muy fuerte, se pierde lo bueno que ya
tenía la solución y es casi como empezar de cero. Se busca un punto medio.

### Prueba
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
