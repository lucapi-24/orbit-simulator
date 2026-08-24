# AGENTS.md — orbit-simulator

## Qué es este proyecto
Simulador orbital 2D educativo. Hay **dos implementaciones paralelas**:

1. **Prototipo Python** (`main.py` + `pyhisics.py`): matplotlib. Sistema Tierra-Luna,
   RK4, maniobras de Hohmann, impulsos prograde/retrograde, telemetría orbital.
   Es el laboratorio donde se validaron las ideas físicas antes de portarlas.
2. **Versión C++ / raylib** (`src/`, `include/`, `CMakeLists.txt`): el juego real.
   Sol + Tierra + asteroide + nave pilotable, cámara con zoom, predicción de
   trayectorias futuras.

Flujo de trabajo: probar la física en Python → portarla a C++.

## Comandos

### C++ (raylib)
```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release  # solo la primera vez
cmake --build build
.\build\orbus.exe
```
- Toolchain: MSYS2 UCRT64 (`g++.exe`, `mingw32-make.exe`). Raylib local en `C:/raylib/raylib`.
- Nota para agente: `cmake` no está en PATH del shell del agente. Para chequear
  sintaxis: prependear `C:\msys64\ucrt64\bin` al PATH y compilar .cpp sueltos con
  `-fsyntax-only` o `-c` (un fichero por llamada).

### Python
```powershell
.\.venv\Scripts\Activate.ps1
python main.py
```
- Depende solo de `numpy` y `matplotlib`. Controles: `espacio` = Hohmann,
  `↑/↓` = prograde/retrograde, `,/.` = velocidad del tiempo.

## Arquitectura C++

```
include/
  physics/PhysicalState.hpp   # struct {position, velocity, mass} (Vector2 de raylib)
  physics/GravityEngine.hpp   # ComputeAcceleration + integradores + PredictTrajectories
  entities/CelestialBody.hpp  # cuerpo celeste base (+ isStatic) + Draw()
  entities/Spaceship.hpp      # hereda de CelestialBody; fuel, angle, thrust
src/
  main.cpp                    # game loop: accumulator → pasos fijos → predicción → draw
  physics/GravityEngine.cpp
  entities/Spaceship.cpp
```

## Física (estado actual, post-refactor 2026-08)

- **Gravedad unificada**: `GravityEngine::ComputeAcceleration(pos, bodies)` es la ÚNICA
  fuente de gravedad. Pura (const), double por dentro / float en la frontera.
  Softening de Plummer: `a = G·M·r_vec/(r²+ε²)^{3/2}` con ε²=25. Auto-exclusión gratis
  (r→0 ⇒ contribución 0).
- **Integrador**: velocity Verlet en `UpdateOrbits` y `UpdateSpaceship`
  (x += v·dt + ½·a₀·dt² → recalcular a₁ → v += ½(a₀+a₁)·dt). Todas las posiciones se
  mueven antes de recalcular aceleraciones (consistencia N-cuerpos).
- **Timestep fijo**: accumulator en `main.cpp`. `FIXED_DT = 1/240`, `MAX_PASOS = 5`,
  cap de tiempo real 0.1 s. Física (input incluido) dentro del bucle fijo;
  cámara/predicción/draw una vez por frame.
- **Sol estático**: flag `CelestialBody::isStatic` (constructor, default false).
  Los estáticos ejercen gravedad pero no se integran ni dibujan órbita predicha.

## Decisiones de diseño (log)

- La nave NO vive en `bodies` (se evitó object slicing); se integra aparte.
  Consecuencia: los planetas no sienten su gravedad (masa despreciable).
- El empuje entra como kick de velocidad en `HandleInput` ANTES del paso Verlet,
  no como parte de la aceleración. Trade-off O(dt²) aceptado; posible mejora futura.
- Predicción reutiliza `UpdateOrbits`/`ComputeAcceleration` (mismo modelo de fuerzas),
  pero corre con su propio dt hardcodeado (0.016f en `main.cpp`) — divergencia fina
  vs simulación (FIXED_DT=1/240) en trayectorias largas. Pulir más adelante.

## Issues

Tracking en GitHub (`lucapi-24/orbit-simulator`). Los 7 issues fundacionales
(bugs + Verlet + refactor gravedad) cerrados en agosto 2026. Convención:
`closes #N` en el commit que llega a `main`.

## Estilo de trabajo con el agente (IMPORTANTE)

El usuario **quiere escribir el código él mismo**. El agente actúa como guía técnica:

- **Modo por defecto: planificación.** Antes de tocar código, proponer plan y esperar confirmación.
- Explicaciones **concretas y concisas**: conceptos, opciones, trade-offs, riesgos.
- **Estilo socrático cuando aplique**: preguntas que guíen en vez de soluciones completas.
- Dar la respuesta directa solo si el usuario la pide explícitamente o si es trivial.
- No escribir código completo salvo petición explícita; preferir pseudocódigo, firmas,
  y señalar archivos/líneas relevantes.
- Comunicarse en español.

## Convenciones

- Comentarios/nombres: mezcla español/inglés (física en español, entidades en inglés).
- Sin framework de tests; verificación visual ejecutando los binarios + chequeo de
  compilación del agente.
- `build/` y `.venv/` gitignoreados; no commitear artefactos.
