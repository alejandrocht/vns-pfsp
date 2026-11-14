// Proyecto ADA - Grupo G2
// Variable Neighborhood Search (VNS) para el Permutation Flow Shop (PFSP).
// Busca el orden de los trabajos que minimiza el makespan.
//
// Compilar:  g++ -O2 -std=c++17 -o vns vns.cpp
// Ejecutar:  ./vns instancias/instancia1_bas1.txt
//
// Aquí está solo el algoritmo. Leer la instancia y guardar los CSV está en utilidades.h.
//
// Una "secuencia" es el orden en que entran los trabajos. Los trabajos se numeran
// desde 0, así que la secuencia {4, 1, 3, 2, 0} significa J5-J2-J4-J3-J1.
#include "utilidades.h"

// ---------------------------------------------------------------------------
// 1. MAKESPAN: cuánto tarda todo el proceso con una secuencia dada.
//
//    Fórmula:  C[i][j] = max(C[i-1][j], C[i][j-1]) + p[i][j]
//    Un trabajo empieza en una máquina cuando se cumplen dos cosas:
//      - la máquina ya terminó el trabajo anterior          (C[i-1][j])
//      - el trabajo ya terminó en la máquina anterior       (C[i][j-1])
// ---------------------------------------------------------------------------
int calcularMakespan(const Instancia& instancia, const vector<int>& secuencia) {
    // finEnMaquina[j] = momento en que la máquina j termina el último trabajo que recibió.
    vector<int> finEnMaquina(instancia.numMaquinas, 0);

    for (int trabajo : secuencia) {
        // En la primera máquina no hay que esperar a ninguna máquina anterior.
        finEnMaquina[0] += instancia.tiempo[trabajo][0];

        for (int maquina = 1; maquina < instancia.numMaquinas; maquina++) {
            int maquinaLibre = finEnMaquina[maquina];       // terminó el trabajo anterior
            int trabajoListo = finEnMaquina[maquina - 1];   // este trabajo salió de la máquina anterior
            int inicio = max(maquinaLibre, trabajoListo);
            finEnMaquina[maquina] = inicio + instancia.tiempo[trabajo][maquina];
        }
    }
    // El makespan es cuando la última máquina termina el último trabajo.
    return finEnMaquina[instancia.numMaquinas - 1];
}

// ---------------------------------------------------------------------------
// 2. VECINDARIOS: las tres formas de cambiar una secuencia.
//
//    N1 SWAP      : intercambia los trabajos de dos posiciones.
//    N2 INSERCION : saca un trabajo de su posición y lo mete en otra.
//    N3 INVERSION : invierte el tramo entre dos posiciones.
// ---------------------------------------------------------------------------
enum Vecindario { SWAP, INSERCION, INVERSION };
const int CANTIDAD_VECINDARIOS = 3;   // el "k max" del VNS

void aplicarMovimiento(vector<int>& secuencia, int vecindario, int posicionA, int posicionB) {
    if (vecindario == SWAP) {
        swap(secuencia[posicionA], secuencia[posicionB]);
    } else if (vecindario == INSERCION) {
        int trabajo = secuencia[posicionA];
        secuencia.erase(secuencia.begin() + posicionA);             // lo sacamos de A
        secuencia.insert(secuencia.begin() + posicionB, trabajo);   // lo metemos en B
    } else {   // INVERSION
        int desde = min(posicionA, posicionB);
        int hasta = max(posicionA, posicionB);
        reverse(secuencia.begin() + desde, secuencia.begin() + hasta + 1);
    }
}

// ---------------------------------------------------------------------------
// 3. BÚSQUEDA LOCAL: baja hasta el óptimo local (ya no hay cambio pequeño que mejore).
// ---------------------------------------------------------------------------

// Prueba TODOS los movimientos posibles de un vecindario y aplica el que deja el
// menor makespan. Devuelve true si encontró uno que mejora, false si ninguno mejora.
bool aplicarMejorMovimiento(const Instancia& instancia, vector<int>& secuencia, int& makespan, int vecindario) {
    int mejorMakespan = makespan;
    int mejorPosicionA = -1, mejorPosicionB = -1;   // -1 = todavía no se encontró mejora

    for (int posicionA = 0; posicionA < instancia.numTrabajos; posicionA++) {
        for (int posicionB = 0; posicionB < instancia.numTrabajos; posicionB++) {
            if (posicionA == posicionB) continue;                           // no cambia nada
            if (vecindario == SWAP && posicionB < posicionA) continue;      // swap(A,B) = swap(B,A)

            vector<int> candidata = secuencia;
            aplicarMovimiento(candidata, vecindario, posicionA, posicionB);
            int makespanCandidata = calcularMakespan(instancia, candidata);

            if (makespanCandidata < mejorMakespan) {
                mejorMakespan = makespanCandidata;
                mejorPosicionA = posicionA;
                mejorPosicionB = posicionB;
            }
        }
    }

    bool huboMejora = (mejorPosicionA != -1);
    if (huboMejora) {
        aplicarMovimiento(secuencia, vecindario, mejorPosicionA, mejorPosicionB);
        makespan = mejorMakespan;
    }
    return huboMejora;
}

// VND: primero se intenta mejorar con inserciones. Solo cuando ninguna inserción
// mejora se prueba con swaps. Si un swap mejora, se vuelve a las inserciones.
// Termina cuando ni la inserción ni el swap logran mejorar.
void busquedaLocal(const Instancia& instancia, vector<int>& secuencia, int& makespan) {
    bool huboMejora = true;
    while (huboMejora) {
        huboMejora = aplicarMejorMovimiento(instancia, secuencia, makespan, INSERCION);
        if (!huboMejora)
            huboMejora = aplicarMejorMovimiento(instancia, secuencia, makespan, SWAP);
    }
}

// ---------------------------------------------------------------------------
// 4. VNS: una corrida completa del algoritmo.
//
//    En cada iteración se recorren los vecindarios N1, N2, N3:
//      a) Shaking        : se desordena un poco la solución actual (empujón al azar).
//      b) Búsqueda local : desde ahí se baja hasta el óptimo local.
//      c) Decisión       : si quedó mejor, se acepta y se vuelve a N1.
//                          si no, se descarta y se pasa al siguiente vecindario.
//
//    Parada: maxSinMejora iteraciones seguidas sin mejorar (o el tope maxIter).
// ---------------------------------------------------------------------------
Resultado ejecutarVNS(const Instancia& instancia, const Parametros& parametros, unsigned semilla) {
    auto horaInicio = chrono::steady_clock::now();
    mt19937 generadorAzar(semilla);   // misma semilla = mismos números al azar = mismo resultado
    uniform_int_distribution<int> posicionAlAzar(0, instancia.numTrabajos - 1);

    // Solución inicial: los trabajos 0, 1, 2, ... mezclados al azar.
    vector<int> solucionActual(instancia.numTrabajos);
    iota(solucionActual.begin(), solucionActual.end(), 0);
    shuffle(solucionActual.begin(), solucionActual.end(), generadorAzar);
    int makespanActual = calcularMakespan(instancia, solucionActual);
    if (parametros.usarBusquedaLocal)
        busquedaLocal(instancia, solucionActual, makespanActual);

    // Historial para la curva de convergencia: el mejor makespan en cada iteración.
    vector<int> historial;
    historial.push_back(makespanActual);   // iteración 0

    int iteraciones = 0;
    int iteracionesSinMejora = 0;
    while (iteraciones < parametros.maxIter && iteracionesSinMejora < parametros.maxSinMejora) {
        iteraciones++;
        iteracionesSinMejora++;

        int vecindario = 0;   // empezamos por N1 (swap)
        while (vecindario < CANTIDAD_VECINDARIOS) {
            // a) Shaking: copiamos la solución actual y le hacemos "fuerza" movimientos al azar.
            vector<int> solucionNueva = solucionActual;
            for (int movimiento = 0; movimiento < parametros.fuerza; movimiento++) {
                int posicionA = posicionAlAzar(generadorAzar);
                int posicionB = posicionAlAzar(generadorAzar);
                if (posicionA == posicionB)   // si salió la misma posición, usamos la siguiente
                    posicionB = (posicionB + 1) % instancia.numTrabajos;
                aplicarMovimiento(solucionNueva, vecindario, posicionA, posicionB);
            }
            int makespanNuevo = calcularMakespan(instancia, solucionNueva);

            // b) Búsqueda local desde la solución desordenada.
            if (parametros.usarBusquedaLocal)
                busquedaLocal(instancia, solucionNueva, makespanNuevo);

            // c) Decisión.
            if (makespanNuevo < makespanActual) {
                solucionActual = solucionNueva;   // mejoró: nos quedamos con la nueva
                makespanActual = makespanNuevo;
                iteracionesSinMejora = 0;         // el contador de parada vuelve a cero
                vecindario = 0;                   // volvemos a N1
            } else {
                vecindario++;                     // no mejoró: probamos el siguiente vecindario
            }
        }
        historial.push_back(makespanActual);
    }

    auto horaFin = chrono::steady_clock::now();

    Resultado resultado;
    resultado.secuencia = solucionActual;
    resultado.makespan = makespanActual;
    resultado.iteraciones = iteraciones;
    resultado.historial = historial;
    resultado.tiempoMs = chrono::duration<double, milli>(horaFin - horaInicio).count();
    return resultado;
}

// ---------------------------------------------------------------------------
// 5. PROGRAMA: lee la instancia, corre el VNS varias veces y guarda los resultados.
//    Se corre varias veces porque el algoritmo usa azar: cada corrida usa una
//    semilla distinta (semilla, semilla + 1, semilla + 2, ...).
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    Parametros parametros = leerOpciones(argc, argv);
    Instancia instancia = leerInstancia(argv[1]);

    vector<Resultado> resultados;
    for (int corrida = 0; corrida < parametros.corridas; corrida++)
        resultados.push_back(ejecutarVNS(instancia, parametros, parametros.semilla + corrida));

    guardarResultados(instancia, parametros, resultados, CANTIDAD_VECINDARIOS);
    return 0;
}
