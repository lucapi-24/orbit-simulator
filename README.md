# Orbit Simulator — Orbus

Educational orbital sandbox con dos pistas paralelas:

- **2D** — Simulador orbital (prototipo Python + juego C++)
- **3D** — Sandbox de planetas procedimentales (en desarrollo)

Ambas comparten raylib. El juego C++ y el sandbox 3D son ejecutables separados.

---

## Compilación unificada

Hay dos scripts que manejan la compilación:

| Script | Qué hace |
|---|---|
| `build.bat` | Compila y ejecuta `orbus.exe` (simulador 2D). Reutiliza build existente si no hay cambios. |
| `build_all.bat` | Compila **ambos**: `orbus.exe` y `proto3d.exe`. Ejecuta `orbus.exe` al final. |

**Uso:**

```powershell
.\build.bat          # Compila orbus.exe y lo ejecuta
.\build_all.bat      # Compila orbus.exe + proto3d.exe y ejecuta orbuns
```

**Notas:**

- El primer uso ejecuta cmake y compila todo. Usos posteriores solo hacen `make` (más rápido).
- Si cambias la configuración (ej. `RAYLIB_PATH`), borra la carpeta `build/` y vuelve a ejecutar el script.
- La ruta de Raylib es configurable: `cmake -DRAYLIB_PATH=ruta/raylib ..`

**Para rebuild completo:**
```powershell
rm -rf build && .\build_all.bat
```

---

## Requisitos

| Dependencia | Notas |
|---|---|
| Windows + MSYS2 UCRT64 | Toolchain: `g++`, `cmake`, `make` |
| raylib (local) | Esperado en `C:/raylib/raylib`, lib estática en `src/libraylib.a` |
| PATH | Prepend `C:\msys64\ucrt64\bin` para tener `cmake`, `make`, `g++` |

---

## Parte 1 — Simulador orbital 2D

### 1.1 C++ game (orbus.exe)

Compilado automáticamente con `build.bat`.

**Controles:**

| Input | Action |
|---|---|
| `W` / `A` / `S` / `D` | Rotar y propulsar |
| Flechas | Igual que WASD |
| Rueda del mouse | Zoom |
| `,` / `.` | Reducir / aumentar warp temporal |
| `C` | Cambiar objetivo de cámara |
| `V` | Cámara libre (luego `WASD` para pan) |
| Botón GUI | Maniobra de Hohmann a la Luna, auto-circulariza |

**Física:**

- Gravedad N-body por medio de `GravityEngine::ComputeAcceleration(pos, bodies)`, `const`, double por dentro / float en el límite.
- Softening de Plummer, ε² = 25: `a = G·M·r_vec/(r² + ε²)^{3/2}`. Auto-exclusión gratuita (`r → 0 ⇒ contribución 0`).
- Integrador Velocity Verlet. Todas las posiciones se advanced antes de recomputar aceleraciones (consistencia N-body).
- Timestep fijo acumulador: `FIXED_DT = 1/240`, `MAX_PASOS = 5`, cap de tiempo real 0.1 s. Física (incluyendo input) corre en el paso fijo; cámara, predicción y draw una vez por frame.
- El Sol es estático (`CelestialBody::isStatic`) — ejerce gravedad pero nunca se integra.

### 1.2 Python prototipo

```powershell
\.venv\Scripts\Activate.ps1
python main.py
```

**Controles:** `space` = Hohmann, `↑` / `↓` = prograde/retrograde, `,` / `.` = velocidad del tiempo.

Sistema Tierra-Luna, RK4, telemetría orbital. Aquí se validan las ideas físicas antes de portar a C++.

---

## Parte 2 — Sandbox 3D procedural

### 2.1 Build and run

Compilado automáticamente con `build_all.bat`.

**Controles:**

| Input | Action |
|---|---|
| `W` `A` `S` `D` | Mover (adelante / izquierda / atrás / derecha) |
| `Q` / `E` | Abajo / arriba |
| Rueda del mouse | Cambiar velocidad |
| `F` | Teletransportarse a la superficie y ver el horizonte (ray-triangle contra mesh real) |

### 2.2 Noise validation test (sin ventana)

Escribe PNGs en `proto/3d/out/` e imprime mediciones en la consola. Compilado con `build_all.bat` (target `noise`).

---

## Estructura del proyecto

```
include/
  physics/PhysicalState.hpp   # {position, velocity, mass} (Vector2 raylib)
  physics/GravityEngine.hpp   # ComputeAcceleration + integradores + PredictTrajectories
  entities/CelestialBody.hpp  # cuerpo base (+ isStatic) + Draw()
  entities/Spaceship.hpp      # hereda de CelestialBody; fuel, angle, thrust
src/
  main.cpp                    # game loop: accumulator → fixed steps → prediction → draw
  physics/GravityEngine.cpp
  entities/Spaceship.cpp
proto/3d/                     # sandbox 3D (ver Parte 2)
```

---

## Convenciones

- Comentarios y nombres: mezcla español/inglés (física en español, entidades en inglés).
- Sin framework de tests: verificación visual ejecutando los binarios + chequeo de compilación.
- `build/`, `.venv/` y `proto/3d/out/` son gitignored — no commitear artefactos generados.