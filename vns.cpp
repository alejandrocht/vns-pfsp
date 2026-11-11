// =============================================================================
//  Proyecto ADA - Grupo G2
//  Variable Neighborhood Search (VNS) aplicado al Permutation Flow Shop (PFSP)
//
//  Objetivo: encontrar el orden de los trabajos que minimiza el makespan
//  (el momento en que termina el último trabajo en la última máquina).
//
//  Compilar:  g++ -O2 -std=c++17 -o vns vns.cpp
//  Ejecutar:  ./vns instancias/instancia1_bas1.txt
//  Opciones:  --corridas N   ejecuciones independientes        (defecto 10)
//             --iter N       tope de iteraciones               (defecto 200)
//             --sinmejora N  parar tras N iteraciones sin mejora (defecto 50)
//             --fuerza N     movimientos por cada shaking      (defecto 2)
//             --semilla N    semilla base; la corrida r usa semilla + r (defecto 1)
//             --bl 0|1       1 = VNS con búsqueda local, 0 = VNS reducido (defecto 1)
//             --salida DIR   carpeta de los CSV                (defecto resultados)
//
//  Los resultados se guardan en <salida>/<instancia>/ en formato CSV.
// =============================================================================
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
namespace fs = std::filesystem;


// -----------------------------------------------------------------------------
// Estructuras principales
// -----------------------------------------------------------------------------

struct Instancia {
    string nombre;
    int n = 0;               // número de trabajos
    int m = 0;               // número de máquinas
    vector<vector<int>> p;   // p[trabajo][máquina] = tiempo de procesamiento
};

struct Parametros {
    int corridas = 10;
    int maxIter = 200;       // tope de seguridad
    int maxSinMejora = 50;   // criterio de parada principal
    int fuerza = 2;          // cuántos movimientos aleatorios hace el shaking
    unsigned semilla = 1;
    bool conBusquedaLocal = true;   // false = VNS reducido (RVNS), solo shaking
    string salida = "resultados";
};

struct Resultado {
    vector<int> secuencia;
    int makespan = 0;
    int iteraciones = 0;
    double tiempoMs = 0;
    unsigned semilla = 0;
    vector<int> historial;   // historial[t] = mejor makespan después de t iteraciones
};

// Una barra del diagrama de Gantt: un trabajo procesándose en una máquina.
struct Operacion {
    int maquina, trabajo, inicio, fin;
};


// -----------------------------------------------------------------------------
// Lectura de la instancia
// Formato: (opcional) una línea de descripción, luego "n m" y después una
// línea por trabajo con pares "máquina tiempo". Las máquinas empiezan en 0.
// -----------------------------------------------------------------------------
Instancia leerInstancia(const string& ruta) {
    ifstream archivo(ruta);
    if (!archivo) {
        cerr << "Error: no se pudo abrir " << ruta << "\n";
        exit(1);
    }

    Instancia inst;
    inst.nombre = fs::path(ruta).stem().string();

    // Buscamos la primera línea que tenga exactamente dos números: n y m.
    string linea;
    while (getline(archivo, linea)) {
        istringstream ss(linea);
        string sobra;
        if ((ss >> inst.n >> inst.m) && !(ss >> sobra)) break;
        inst.n = inst.m = 0;
    }
    if (inst.n <= 0 || inst.m <= 0) {
        cerr << "Error: no se encontró la línea \"n m\" en " << ruta << "\n";
        exit(1);
    }

    inst.p.assign(inst.n, vector<int>(inst.m, 0));
    for (int t = 0; t < inst.n; t++) {
        for (int k = 0; k < inst.m; k++) {
            int maquina, tiempo;
            if (!(archivo >> maquina >> tiempo) || maquina < 0 || maquina >= inst.m) {
                cerr << "Error: datos inválidos en el trabajo " << t + 1 << "\n";
                exit(1);
            }
            inst.p[t][maquina] = tiempo;
        }
    }
    return inst;
}


// -----------------------------------------------------------------------------
// Función objetivo: makespan
// C[i][j] = max(C[i-1][j], C[i][j-1]) + p[i][j]
// No hace falta guardar toda la tabla: c[j] guarda cuándo se libera la máquina j
// y se va actualizando trabajo por trabajo.
// -----------------------------------------------------------------------------
int calcularMakespan(const Instancia& inst, const vector<int>& sec) {
    vector<int> c(inst.m, 0);
    for (int trabajo : sec) {
        c[0] += inst.p[trabajo][0];
        for (int j = 1; j < inst.m; j++)
            c[j] = max(c[j], c[j - 1]) + inst.p[trabajo][j];
    }
    return c[inst.m - 1];
}

// Misma fórmula, pero guardando inicio y fin de cada operación para el Gantt.
vector<Operacion> calcularGantt(const Instancia& inst, const vector<int>& sec) {
    vector<vector<int>> C(inst.n, vector<int>(inst.m, 0));
    vector<Operacion> ops;

    for (int i = 0; i < inst.n; i++) {
        for (int j = 0; j < inst.m; j++) {
            int maquinaLibre = (i > 0) ? C[i - 1][j] : 0;   // terminó el trabajo anterior
            int trabajoListo = (j > 0) ? C[i][j - 1] : 0;   // terminó en la máquina anterior
            int inicio = max(maquinaLibre, trabajoListo);
            C[i][j] = inicio + inst.p[sec[i]][j];
            ops.push_back({j, sec[i], inicio, C[i][j]});
        }
    }
    return ops;
}


// -----------------------------------------------------------------------------
// Vecindarios sobre la permutación
//   N1 swap      : intercambia los trabajos de las posiciones i y j
//   N2 inserción : saca el trabajo de la posición i y lo mete en la j
//   N3 inversión : invierte el tramo entre i y j
// -----------------------------------------------------------------------------
enum Vecindario { SWAP, INSERCION, INVERSION };
const int K_MAX = 3;

void aplicarMovimiento(vector<int>& sec, Vecindario tipo, int i, int j) {
    switch (tipo) {
        case SWAP:
            swap(sec[i], sec[j]);
            break;
        case INSERCION: {
            int trabajo = sec[i];
            sec.erase(sec.begin() + i);
            sec.insert(sec.begin() + j, trabajo);
            break;
        }
        case INVERSION:
            if (i > j) swap(i, j);
            reverse(sec.begin() + i, sec.begin() + j + 1);
            break;
    }
}


// -----------------------------------------------------------------------------
// Búsqueda local (VND)
// Primero prueba todas las inserciones y aplica la mejor. Cuando la inserción
// ya no mejora, prueba los swaps. Si un swap mejora, vuelve a las inserciones.
// Termina cuando la solución es un óptimo local en ambos vecindarios.
// -----------------------------------------------------------------------------
void busquedaLocal(const Instancia& inst, vector<int>& sec, int& costo) {
    const Vecindario orden[2] = {INSERCION, SWAP};
    int v = 0;

    while (v < 2) {
        Vecindario tipo = orden[v];
        int mejorCosto = costo, mejorI = -1, mejorJ = -1;

        for (int i = 0; i < inst.n; i++) {
            for (int j = 0; j < inst.n; j++) {
                if (i == j) continue;
                if (tipo == SWAP && j < i) continue;   // swap(i,j) = swap(j,i)

                vector<int> vecino = sec;
                aplicarMovimiento(vecino, tipo, i, j);
                int c = calcularMakespan(inst, vecino);
                if (c < mejorCosto) {
                    mejorCosto = c;
                    mejorI = i;
                    mejorJ = j;
                }
            }
        }

        if (mejorI != -1) {
            aplicarMovimiento(sec, tipo, mejorI, mejorJ);
            costo = mejorCosto;
            v = 0;          // hubo mejora: volvemos al primer vecindario
        } else {
            v++;            // sin mejora: pasamos al siguiente
        }
    }
}


// -----------------------------------------------------------------------------
// VNS
//   s = solución inicial aleatoria + búsqueda local
//   repetir hasta cumplir el criterio de parada:
//       k = 1
//       mientras k <= K_MAX:
//           s'  = shaking(s, N_k)
//           s'' = busquedaLocal(s')
//           si makespan(s'') < makespan(s):  s = s'' ; k = 1
//           si no:                           k = k + 1
//   Parada: maxSinMejora iteraciones seguidas sin mejorar, o maxIter.
// -----------------------------------------------------------------------------
Resultado ejecutarVNS(const Instancia& inst, const Parametros& par, unsigned semilla) {
    auto inicio = chrono::steady_clock::now();
    mt19937 rng(semilla);
    uniform_int_distribution<int> posicion(0, inst.n - 1);

    // Solución inicial aleatoria, mejorada con búsqueda local.
    vector<int> actual(inst.n);
    iota(actual.begin(), actual.end(), 0);
    shuffle(actual.begin(), actual.end(), rng);
    int costoActual = calcularMakespan(inst, actual);
    if (par.conBusquedaLocal) busquedaLocal(inst, actual, costoActual);

    Resultado res;
    res.semilla = semilla;
    res.historial.push_back(costoActual);   // iteración 0

    int iter = 0, sinMejora = 0;
    while (iter < par.maxIter && sinMejora < par.maxSinMejora) {
        iter++;
        bool mejoro = false;
        int k = 0;

        while (k < K_MAX) {
            // Shaking: perturbamos la solución actual dentro del vecindario k
            // para salir del óptimo local en el que estamos.
            vector<int> vecino = actual;
            for (int q = 0; q < par.fuerza; q++) {
                int i = posicion(rng), j = posicion(rng);
                if (i == j) j = (j + 1) % inst.n;
                aplicarMovimiento(vecino, Vecindario(k), i, j);
            }

            int costoVecino = calcularMakespan(inst, vecino);
            if (par.conBusquedaLocal) busquedaLocal(inst, vecino, costoVecino);

            // Cambio de vecindario.
            if (costoVecino < costoActual) {
                actual = vecino;
                costoActual = costoVecino;
                mejoro = true;
                k = 0;
            } else {
                k++;
            }
        }

        sinMejora = mejoro ? 0 : sinMejora + 1;
        res.historial.push_back(costoActual);
    }

    res.secuencia = actual;
    res.makespan = costoActual;
    res.iteraciones = iter;
    res.tiempoMs = chrono::duration<double, milli>(chrono::steady_clock::now() - inicio).count();
    return res;
}


// -----------------------------------------------------------------------------
// Validación: óptimo exacto probando todas las permutaciones.
// Solo es viable con pocos trabajos (10! = 3.6 millones; 12! ya son 479 millones).
// -----------------------------------------------------------------------------
int optimoExacto(const Instancia& inst, vector<int>& mejorSec) {
    vector<int> sec(inst.n);
    iota(sec.begin(), sec.end(), 0);
    int mejor = calcularMakespan(inst, sec);
    mejorSec = sec;

    while (next_permutation(sec.begin(), sec.end())) {
        int c = calcularMakespan(inst, sec);
        if (c < mejor) {
            mejor = c;
            mejorSec = sec;
        }
    }
    return mejor;
}


// -----------------------------------------------------------------------------
// Utilidades de salida
// -----------------------------------------------------------------------------

// [4, 1, 0] -> "J5-J2-J1"
string secuenciaATexto(const vector<int>& sec) {
    string texto;
    for (size_t i = 0; i < sec.size(); i++) {
        if (i > 0) texto += "-";
        texto += "J" + to_string(sec[i] + 1);
    }
    return texto;
}

string parametrosATexto(const Parametros& par) {
    return "maxIter=" + to_string(par.maxIter) + ";sinMejora=" + to_string(par.maxSinMejora) +
           ";fuerza=" + to_string(par.fuerza) + ";kmax=" + to_string(K_MAX) +
           (par.conBusquedaLocal ? ";BL=VND(insercion+swap)" : ";BL=ninguna(RVNS)") + ";inicial=aleatoria";
}

void guardarCorridas(const fs::path& dir, const vector<Resultado>& res, const Parametros& par) {
    ofstream f(dir / "corridas.csv");
    f << "corrida,semilla,makespan,tiempo_ms,iteraciones,secuencia,parametros\n";
    for (size_t r = 0; r < res.size(); r++)
        f << r + 1 << ',' << res[r].semilla << ',' << res[r].makespan << ',' << res[r].tiempoMs << ','
          << res[r].iteraciones << ',' << secuenciaATexto(res[r].secuencia) << ',' << parametrosATexto(par) << '\n';
}

// Una columna por corrida. Como cada corrida para en una iteración distinta,
// las más cortas se completan repitiendo su último valor.
void guardarConvergencia(const fs::path& dir, const vector<Resultado>& res) {
    size_t largo = 0;
    for (const auto& r : res) largo = max(largo, r.historial.size());

    ofstream f(dir / "convergencia.csv");
    f << "iteracion";
    for (size_t r = 0; r < res.size(); r++) f << ",corrida" << r + 1;
    f << '\n';

    for (size_t t = 0; t < largo; t++) {
        f << t;
        for (const auto& r : res) f << ',' << r.historial[min(t, r.historial.size() - 1)];
        f << '\n';
    }
}

void guardarGantt(const fs::path& dir, const Instancia& inst, const vector<int>& sec) {
    ofstream f(dir / "gantt.csv");
    f << "maquina,trabajo,inicio,fin\n";
    for (const auto& op : calcularGantt(inst, sec))
        f << 'M' << op.maquina + 1 << ",J" << op.trabajo + 1 << ',' << op.inicio << ',' << op.fin << '\n';
}


// -----------------------------------------------------------------------------
// Programa principal
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << " instancia.txt [--corridas N] [--iter N] [--sinmejora N] [--fuerza N] [--semilla N] [--bl 0|1] [--salida DIR]\n";
        return 1;
    }

    // Las opciones vienen en pares "--nombre valor".
    if ((argc - 2) % 2 != 0) {
        cerr << "Error: cada opción necesita un valor (ej. --corridas 10)\n";
        return 1;
    }

    Parametros par;
    for (int a = 2; a + 1 < argc; a += 2) {
        string opcion = argv[a];
        if (opcion == "--salida") { par.salida = argv[a + 1]; continue; }

        int valor = stoi(argv[a + 1]);
        if      (opcion == "--corridas")  par.corridas = valor;
        else if (opcion == "--bl")        par.conBusquedaLocal = (valor != 0);
        else if (opcion == "--iter")      par.maxIter = valor;
        else if (opcion == "--sinmejora") par.maxSinMejora = valor;
        else if (opcion == "--fuerza")    par.fuerza = valor;
        else if (opcion == "--semilla")   par.semilla = valor;
        else { cerr << "Opción desconocida: " << opcion << "\n"; return 1; }
    }

    Instancia inst = leerInstancia(argv[1]);
    fs::path dir = fs::path(par.salida) / inst.nombre;
    fs::create_directories(dir);

    cout << "Instancia " << inst.nombre << " (" << inst.n << " trabajos x " << inst.m << " máquinas)\n";
    cout << "Parámetros: " << parametrosATexto(par) << ", corridas=" << par.corridas << "\n\n";

    // 1. Ejecutamos las corridas independientes.
    vector<Resultado> res;
    for (int r = 0; r < par.corridas; r++) {
        res.push_back(ejecutarVNS(inst, par, par.semilla + r));
        const Resultado& x = res.back();
        printf("Corrida %2d | makespan %5d | %7.2f ms | %3d iter | %s\n",
               r + 1, x.makespan, x.tiempoMs, x.iteraciones, secuenciaATexto(x.secuencia).c_str());
    }

    // 2. Estadísticas: mejor, peor, promedio, desviación estándar y tiempo promedio.
    int idxMejor = 0, peor = res[0].makespan;
    double suma = 0, sumaTiempo = 0;
    for (int r = 0; r < par.corridas; r++) {
        if (res[r].makespan < res[idxMejor].makespan) idxMejor = r;
        peor = max(peor, res[r].makespan);
        suma += res[r].makespan;
        sumaTiempo += res[r].tiempoMs;
    }
    double promedio = suma / par.corridas;
    double varianza = 0;
    for (const auto& x : res) varianza += (x.makespan - promedio) * (x.makespan - promedio);
    double desviacion = par.corridas > 1 ? sqrt(varianza / (par.corridas - 1)) : 0;   // muestral
    double tiempoPromedio = sumaTiempo / par.corridas;
    const Resultado& mejor = res[idxMejor];

    cout << "\n=== Resumen " << inst.nombre << " ===\n"
         << "Mejor makespan  : " << mejor.makespan << "\n"
         << "Peor makespan   : " << peor << "\n"
         << "Promedio        : " << promedio << "\n"
         << "Desv. estándar  : " << desviacion << "\n"
         << "Tiempo promedio : " << tiempoPromedio << " ms\n"
         << "Mejor secuencia : " << secuenciaATexto(mejor.secuencia) << "\n";

    // 3. Si la instancia es pequeña, comprobamos contra el óptimo exacto.
    string optimo = "NA";
    if (inst.n <= 10) {
        vector<int> secOptima;
        int valor = optimoExacto(inst, secOptima);
        optimo = to_string(valor);
        cout << "Óptimo exacto   : " << valor << " (" << secuenciaATexto(secOptima) << ")"
             << (valor == mejor.makespan ? "  -> VNS lo alcanzó\n" : "  -> VNS no lo alcanzó\n");
    }

    // 4. Guardamos todo para el informe y los gráficos.
    guardarCorridas(dir, res, par);
    guardarConvergencia(dir, res);
    guardarGantt(dir, inst, mejor.secuencia);

    ofstream resumen(dir / "resumen.csv");
    resumen << "instancia,mejor,peor,promedio,desv_estandar,tiempo_prom_ms,mejor_secuencia,optimo\n"
            << inst.nombre << ',' << mejor.makespan << ',' << peor << ',' << promedio << ',' << desviacion << ','
            << tiempoPromedio << ',' << secuenciaATexto(mejor.secuencia) << ',' << optimo << '\n';

    cout << "\nArchivos guardados en " << dir.string() << "/\n";
    return 0;
}
