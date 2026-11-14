// Proyecto ADA - Grupo G2
// Variable Neighborhood Search (VNS) para el Permutation Flow Shop (PFSP).
// Busca el orden de los trabajos que minimiza el makespan.
//
// Compilar:  g++ -O2 -std=c++17 -o vns vns.cpp
// Ejecutar:  ./vns instancias/instancia1_bas1.txt
//
// Aquí está solo el algoritmo. Leer la instancia y guardar los CSV está en utilidades.h.
#include "utilidades.h"

// 1. MAKESPAN: cuánto tarda todo el proceso con un orden dado.
//    C[i][j] = max(C[i-1][j], C[i][j-1]) + p[i][j]
//    c[j] guarda cuándo se libera la máquina j.
int calcularMakespan(const Instancia& inst, const vector<int>& sec) {
    vector<int> c(inst.m, 0);
    for (int trabajo : sec) {
        c[0] += inst.p[trabajo][0];
        for (int j = 1; j < inst.m; j++)
            c[j] = max(c[j], c[j - 1]) + inst.p[trabajo][j];
    }
    return c[inst.m - 1];
}

// 2. VECINDARIOS: las tres formas de cambiar un orden.
//    N1 swap      : intercambia los trabajos de las posiciones i y j
//    N2 inserción : saca el trabajo de la posición i y lo mete en la j
//    N3 inversión : invierte el tramo entre i y j
enum Vecindario { SWAP, INSERCION, INVERSION, K_MAX };

void aplicarMovimiento(vector<int>& sec, int tipo, int i, int j) {
    if (tipo == SWAP) {
        swap(sec[i], sec[j]);
    } else if (tipo == INSERCION) {
        int trabajo = sec[i];
        sec.erase(sec.begin() + i);
        sec.insert(sec.begin() + j, trabajo);
    } else {
        reverse(sec.begin() + min(i, j), sec.begin() + max(i, j) + 1);
    }
}

// 3. BÚSQUEDA LOCAL (VND): baja hasta el óptimo local.
//    Prueba todas las inserciones y aplica la mejor, mientras mejore.
//    Cuando ya no mejora, prueba el mejor swap; si mejora, vuelve a las inserciones.
void busquedaLocal(const Instancia& inst, vector<int>& sec, int& costo) {
    const int orden[2] = {INSERCION, SWAP};
    for (int v = 0; v < 2;) {
        int mejorI = -1, mejorJ = -1;
        for (int i = 0; i < inst.n; i++)
            for (int j = 0; j < inst.n; j++) {
                if (i == j || (orden[v] == SWAP && j < i)) continue;   // swap(i,j) = swap(j,i)
                vector<int> vecino = sec;
                aplicarMovimiento(vecino, orden[v], i, j);
                int c = calcularMakespan(inst, vecino);
                if (c < costo) { costo = c; mejorI = i; mejorJ = j; }
            }
        if (mejorI == -1) { v++; continue; }   // sin mejora: siguiente vecindario
        aplicarMovimiento(sec, orden[v], mejorI, mejorJ);
        v = 0;                                 // hubo mejora: volvemos a inserción
    }
}

// 4. VNS: en cada iteración se recorren los vecindarios k = N1, N2, N3.
//      s'  = shaking(s, N_k)       -> empujón al azar
//      s'' = busquedaLocal(s')     -> bajada
//      si s'' es mejor: s = s'' y se vuelve a N1;  si no: se pasa a N(k+1)
//    Parada: maxSinMejora iteraciones seguidas sin mejorar, o maxIter.
Resultado ejecutarVNS(const Instancia& inst, const Parametros& par, unsigned semilla) {
    auto inicio = chrono::steady_clock::now();
    mt19937 rng(semilla);
    uniform_int_distribution<int> posicion(0, inst.n - 1);
    Resultado res;
    vector<int>& actual = res.secuencia;   // la solución actual es la que se devuelve
    int& costoActual = res.makespan;

    // Solución inicial: orden al azar, mejorado con búsqueda local.
    actual.resize(inst.n);
    iota(actual.begin(), actual.end(), 0);
    shuffle(actual.begin(), actual.end(), rng);
    costoActual = calcularMakespan(inst, actual);
    if (par.bl) busquedaLocal(inst, actual, costoActual);
    res.historial.push_back(costoActual);

    for (int sinMejora = 0; res.iteraciones < par.maxIter && sinMejora < par.maxSinMejora;) {
        res.iteraciones++;
        sinMejora++;
        for (int k = 0; k < K_MAX;) {
            // Shaking: "fuerza" movimientos al azar del vecindario k.
            vector<int> vecino = actual;
            for (int q = 0; q < par.fuerza; q++) {
                int i = posicion(rng), j = posicion(rng);
                if (i == j) j = (j + 1) % inst.n;
                aplicarMovimiento(vecino, k, i, j);
            }
            int costoVecino = calcularMakespan(inst, vecino);
            if (par.bl) busquedaLocal(inst, vecino, costoVecino);

            if (costoVecino < costoActual) {   // mejoró: aceptamos y volvemos a N1
                actual = vecino;
                costoActual = costoVecino;
                sinMejora = k = 0;
            } else {
                k++;                           // no mejoró: siguiente vecindario
            }
        }
        res.historial.push_back(costoActual);
    }
    res.tiempoMs = chrono::duration<double, milli>(chrono::steady_clock::now() - inicio).count();
    return res;
}

// 5. PROGRAMA: lee la instancia, corre el VNS varias veces y guarda los resultados.
int main(int argc, char** argv) {
    Parametros par = leerOpciones(argc, argv);
    Instancia inst = leerInstancia(argv[1]);
    vector<Resultado> res;
    for (int r = 0; r < par.corridas; r++)
        res.push_back(ejecutarVNS(inst, par, par.semilla + r));
    guardarResultados(inst, par, res, K_MAX);
    return 0;
}
