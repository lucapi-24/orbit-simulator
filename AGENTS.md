# AGENTS.md — orbit-simulator

## Qué es este proyecto
Simulador orbital 2D educativo. Hay **dos implementaciones paralelas**:

1. **Prototipo Python** (`main.py` + `pyhisics.py`): matplotlib. Sistema Tierra-Luna,
   integrador RK4, maniobras de Hohmann (tecla espacio), impulsos prograde/retrograde
   (flechas), predicción de trayectoria cada frame, telemetría orbital (periapsis,
   apoapsis, excentricidad, energía). Es el laboratorio donde se validan las ideas físicas.
2. **Versión C++ / raylib** (`src/`, `include/`, `CMakeLists.txt`): el juego real.
   Sol + Tierra + asteroide + nave pilotable (rotar/acelerar/consumo de combustible),
   cámara que sigue a la nave con zoom, predicción de trayectorias futuras.

Flujo de trabajo implícito: probar la física en Python → portarla a C++.

## Comandos

### C++ (raylib)
```powershell
# Configurar (solo la primera vez o al cambiar CMakeLists.txt)
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# Compilar
cmake --build build

# Ejecutar
.\build\orbus.exe
```
- Toolchain: MSYS2 UCRT64 (`g++.exe`, `mingw32-make.exe`).
- Raylib está instalada localmente en `C:/raylib/raylib` (ruta hardcodeada en CMakeLists.txt).

### Python
```powershell
.\.venv\Scripts\Activate.ps1
python main.py
```
- Depende solo de `numpy` y `matplotlib`.
- Controles: `espacio` = Hohmann, `↑/↓` = prograde/retrograde, `,/.` = velocidad del tiempo.

## Arquitectura C++

```
include/
  physics/PhysicalState.hpp   # struct {position, velocity, mass} (Vector2 de raylib)
  physics/GravityEngine.hpp   # estáticos: UpdateOrbits, UpdateSpaceship, PredictTrajectories
  entities/CelestialBody.hpp  # cuerpo celeste base (name, physics, radius, color) + Draw()
  entities/Spaceship.hpp      # hereda de CelestialBody; fuel, angle, thrust
src/
  main.cpp                    # game loop raylib: input → física → predicción → draw
  physics/GravityEngine.cpp   # N-cuerpos O(n²) Euler semi-implícito
  entities/Spaceship.cpp      # input de nave + dibujo triangular
```

Física actual en C++: integración **Euler** (v += a·dt; x += v·dt). El prototipo Python
usa **RK4** (`rk4_step` en pyhisics.py) — portar RK4 a C++ es una mejora pendiente obvia.

## Bugs conocidos (no arreglados todavía)

- `main.cpp:40`: `bodies.push_back(playerShip)` hace **object slicing** (Spaceship → CelestialBody).
  Funciona porque luego se copia `.physics` de vuelta, pero es frágil.
- `main.cpp:55` y `GravityEngine.cpp:50`: comparan `name == "Sol"` pero el cuerpo se llama
  `"Sun"` → la lógica "el sol no se mueve" nunca se activa.
- `GravityEngine.cpp:117`: en PredictTrajectories hay un `10.0` hardcodeado en vez de usar `G`.
- `Spaceship.cpp:23`: precedencia de operadores — `a or b && c` evalúa como `a or (b && c)`;
  la nave puede acelerar sin combustible.
- `main.py:237`: `t += dt` duplicado (ya se incrementa dentro del bucle de sub-steps).
- `pyhisics.py`: nombre con typo (debería ser `physics.py`); `utils.py` y `README.md` vacíos.

## Estilo de trabajo con el agente (IMPORTANTE)

El usuario **quiere escribir el código él mismo**. El agente actúa como guía técnica:

- **Modo por defecto: planificación.** Antes de tocar código, proponer plan y esperar confirmación.
- Explicaciones **concretas y concisas**: conceptos, opciones, trade-offs, riesgos.
- **Estilo socrático cuando aplique**: en vez de dar la solución completa, hacer preguntas
  que guíen ("¿qué pasa con h si aplicas el impulso en apoapsis?") para que el usuario razone.
- Dar la respuesta directa solo si el usuario la pide explícitamente o si es trivial.
- No escribir código completo salvo petición explícita; preferir pseudocódigo, firmas,
  y señalar archivos/líneas relevantes.
- Comunicarse en español.

## Convenciones

- Comentarios y nombres en el código: mezcla español/inglés (física en español, entidades en inglés).
- Sin framework de tests; la verificación es visual/ejecutando los binarios.
- `build/` y `.venv/` están gitignoreados; no commitear artefactos.
