// Proyecto ADA - Grupo G2
// Todo lo que NO es el algoritmo: estructuras de datos, lectura de la instancia,
// opciones de la línea de comandos, estadísticas y guardado de los CSV.
// El algoritmo VNS está en vns.cpp.
#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>
using namespace std;
namespace fs = std::filesystem;

struct Instancia {
    string nombre;
    int numTrabajos = 0, numMaquinas = 0;
    vector<vector<int>> tiempo;   // tiempo[trabajo][máquina] = tiempo de procesamiento
};

struct Parametros {
    int corridas = 10;
    int maxIter = 200;       // tope de seguridad
    int maxSinMejora = 50;   // criterio de parada principal
    int fuerza = 2;          // movimientos aleatorios por shaking
    int semilla = 1;         // la corrida r usa semilla + r
    int usarBusquedaLocal = 1;   // 1 = con búsqueda local, 0 = VNS reducido (RVNS)
    string salida = "resultados";
};

struct Resultado {
    vector<int> secuencia, historial;   // historial = mejor makespan tras cada iteración
    int makespan = 0, iteraciones = 0;
    double tiempoMs = 0;
};

int calcularMakespan(const Instancia& inst, const vector<int>& sec);   // definida en vns.cpp

// Formato: "n m" y luego una línea por trabajo con pares "máquina tiempo" (máquinas desde 0).
Instancia leerInstancia(const string& ruta) {
    ifstream archivo(ruta);
    Instancia inst;
    inst.nombre = fs::path(ruta).stem().string();
    string linea, sobra;
    while (getline(archivo, linea)) {   // salta la descripción hasta la línea "n m"
        istringstream ss(linea);
        if ((ss >> inst.numTrabajos >> inst.numMaquinas) && !(ss >> sobra)) break;
        inst.numTrabajos = inst.numMaquinas = 0;
    }
    bool ok = inst.numTrabajos > 0 && inst.numMaquinas > 0;
    inst.tiempo.assign(inst.numTrabajos, vector<int>(inst.numMaquinas, 0));
    for (int t = 0; ok && t < inst.numTrabajos; t++)
        for (int k = 0, maquina, tiempo; ok && k < inst.numMaquinas; k++) {
            ok = (archivo >> maquina >> tiempo) && maquina >= 0 && maquina < inst.numMaquinas;
            if (ok) inst.tiempo[t][maquina] = tiempo;
        }
    if (!ok) { cerr << "Error: no se pudo leer la instancia " << ruta << "\n"; exit(1); }
    return inst;
}

// Opciones en pares "--nombre valor". Si algo está mal, muestra el uso y termina.
Parametros leerOpciones(int argc, char** argv) {
    Parametros par;
    map<string, int*> opciones = {{"--corridas", &par.corridas}, {"--iter", &par.maxIter},
                                  {"--sinmejora", &par.maxSinMejora}, {"--fuerza", &par.fuerza},
                                  {"--semilla", &par.semilla}, {"--bl", &par.usarBusquedaLocal}};
    bool uso = argc < 2 || argc % 2 != 0;
    for (int a = 2; !uso && a < argc; a += 2) {
        string opcion = argv[a];
        if (opcion == "--salida") par.salida = argv[a + 1];
        else if (opciones.count(opcion)) *opciones[opcion] = stoi(argv[a + 1]);
        else uso = true;
    }
    if (uso) { cerr << "Uso: ./vns instancia.txt [--corridas N] [--iter N] [--sinmejora N] [--fuerza N] [--semilla N] [--bl 0|1] [--salida DIR]\n"; exit(1); }
    return par;
}

// [4, 1, 0] -> "J5-J2-J1"
string secuenciaATexto(const vector<int>& sec) {
    string texto;
    for (int trabajo : sec) texto += (texto.empty() ? "J" : "-J") + to_string(trabajo + 1);
    return texto;
}

// Muestra las corridas, calcula las estadísticas, valida contra el óptimo y guarda los CSV.
void guardarResultados(const Instancia& inst, const Parametros& par, const vector<Resultado>& res, int kmax) {
    fs::path dir = fs::path(par.salida) / inst.nombre;
    fs::create_directories(dir);
    string parametros = "maxIter=" + to_string(par.maxIter) + ";sinMejora=" + to_string(par.maxSinMejora) +
                        ";fuerza=" + to_string(par.fuerza) + ";kmax=" + to_string(kmax) +
                        (par.usarBusquedaLocal ? ";BL=VND(insercion+swap)" : ";BL=ninguna(RVNS)") + ";inicial=aleatoria";
    cout << "Instancia " << inst.nombre << " (" << inst.numTrabajos << " trabajos x " << inst.numMaquinas << " máquinas)\n"
         << "Parámetros: " << parametros << ", corridas=" << par.corridas << "\n\n";

    // 1. Corridas y estadísticas.
    ofstream fCorridas(dir / "corridas.csv");
    fCorridas << "corrida,semilla,makespan,tiempo_ms,iteraciones,secuencia,parametros\n";
    int mejor = 0, peor = 0;   // mejor = índice de la mejor corrida; peor = peor makespan
    double promedio = 0, tiempoPromedio = 0, varianza = 0;
    for (int r = 0; r < par.corridas; r++) {
        const Resultado& x = res[r];
        string sec = secuenciaATexto(x.secuencia);
        printf("Corrida %2d | makespan %5d | %7.2f ms | %3d iter | %s\n", r + 1, x.makespan, x.tiempoMs, x.iteraciones, sec.c_str());
        fCorridas << r + 1 << ',' << par.semilla + r << ',' << x.makespan << ',' << x.tiempoMs << ','
                  << x.iteraciones << ',' << sec << ',' << parametros << '\n';
        if (x.makespan < res[mejor].makespan) mejor = r;
        peor = max(peor, x.makespan);
        promedio += x.makespan;
        tiempoPromedio += x.tiempoMs;
    }
    promedio /= par.corridas;
    tiempoPromedio /= par.corridas;
    for (const auto& x : res) varianza += (x.makespan - promedio) * (x.makespan - promedio);
    double desviacion = par.corridas > 1 ? sqrt(varianza / (par.corridas - 1)) : 0;   // muestral
    const vector<int>& mejorSec = res[mejor].secuencia;
    int mejorMakespan = res[mejor].makespan;

    cout << "\n=== Resumen " << inst.nombre << " ===\n"
         << "Mejor makespan  : " << mejorMakespan << "\n"
         << "Peor makespan   : " << peor << "\n"
         << "Promedio        : " << promedio << "\n"
         << "Desv. estándar  : " << desviacion << "\n"
         << "Tiempo promedio : " << tiempoPromedio << " ms\n"
         << "Mejor secuencia : " << secuenciaATexto(mejorSec) << "\n";

    // 2. Validación: con n <= 10 se prueban todas las permutaciones (10! = 3.6 millones).
    string optimo = "NA";
    if (inst.numTrabajos <= 10) {
        vector<int> sec(inst.numTrabajos), secOptima;
        iota(sec.begin(), sec.end(), 0);
        int valor = calcularMakespan(inst, secOptima = sec);
        while (next_permutation(sec.begin(), sec.end())) {
            int c = calcularMakespan(inst, sec);
            if (c < valor) { valor = c; secOptima = sec; }
        }
        optimo = to_string(valor);
        cout << "Óptimo exacto   : " << valor << " (" << secuenciaATexto(secOptima) << ")"
             << (valor == mejorMakespan ? "  -> VNS lo alcanzó\n" : "  -> VNS no lo alcanzó\n");
    }

    // 3. Convergencia: una columna por corrida; las más cortas repiten su último valor.
    size_t largo = 0;
    for (const auto& x : res) largo = max(largo, x.historial.size());
    ofstream fConv(dir / "convergencia.csv");
    fConv << "iteracion";
    for (int r = 0; r < par.corridas; r++) fConv << ",corrida" << r + 1;
    for (size_t t = 0; t < largo; t++) {
        fConv << '\n' << t;
        for (const auto& x : res) fConv << ',' << x.historial[min(t, x.historial.size() - 1)];
    }
    fConv << '\n';

    // 4. Gantt de la mejor secuencia: misma fórmula del makespan, guardando inicio y fin.
    ofstream fGantt(dir / "gantt.csv");
    fGantt << "maquina,trabajo,inicio,fin\n";
    vector<int> c(inst.numMaquinas, 0);
    for (int trabajo : mejorSec)
        for (int j = 0; j < inst.numMaquinas; j++) {
            int inicio = max(c[j], j > 0 ? c[j - 1] : 0);   // máquina libre y trabajo listo
            c[j] = inicio + inst.tiempo[trabajo][j];
            fGantt << 'M' << j + 1 << ",J" << trabajo + 1 << ',' << inicio << ',' << c[j] << '\n';
        }

    ofstream(dir / "resumen.csv")
        << "instancia,mejor,peor,promedio,desv_estandar,tiempo_prom_ms,mejor_secuencia,optimo\n"
        << inst.nombre << ',' << mejorMakespan << ',' << peor << ',' << promedio << ',' << desviacion << ','
        << tiempoPromedio << ',' << secuenciaATexto(mejorSec) << ',' << optimo << '\n';
    cout << "\nArchivos guardados en " << dir.string() << "/\n";
}
