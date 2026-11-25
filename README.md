# Orbit Simulator — Orbus

Educational orbital sandbox con dos pistas paralelas:

- **2D** — Simulador orbital (prototipo Python + juego C++)
- **3D** — Sandbox de planetas procedimentales (en desarrollo)

Ambas comparten raylib. El juego C++ y el sandbox 3D son ejecutables separados.

---

## Compilación

Se usa CMake con el generador MinGW Makefiles. Configura el proyecto una vez:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
```

Para compilar todos los ejecutables o solo uno:

```powershell
cmake --build build
cmake --build build --target orbus
cmake --build build --target proto3d
cmake --build build --target noise_test
```

Para usar Raylib desde otra ubicación, indícala al configurar:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DRAYLIB_PATH="C:/ruta/a/raylib"
```

Después de compilar, ejecuta el programa desde PowerShell, por ejemplo `.\build\orbus.exe` o `.\build\proto3d.exe`.

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

Compila con `cmake --build build --target orbus`.

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

Compila con `cmake --build build --target proto3d`.

**Controles:**

| Input | Action |
|---|---|
| `W` `A` `S` `D` | Mover (adelante / izquierda / atrás / derecha) |
| `Q` / `E` | Abajo / arriba |
| Rueda del mouse | Cambiar velocidad |
| `F` | Teletransportarse a la superficie y ver el horizonte (ray-triangle contra mesh real) |

### 2.2 Mejoras visuales en esta versión

Se han realizado dos cambios clave en `proto/3d/main3d.cpp` para mejorar la percepción de relieve y eliminar los aspectos "cuadrados":

**1. Desplazamiento de vértices con ruido 3D (en lugar de 2D UV):**

- **Antes:** `HeightAt()` muestreaba cuadrículas 2D sobre coordenadas de textura (UV). Cada celda medía ~1484 km a escala del planeta, creando patrones repetitivos "a cuadrado" visibles desde cualquier distancia.

- **Después:** `Altitud3D(dir)` evalúa `Noise3D()` sobre el vector dirección unitario del vértice. Esto proporciona:
  - Escala continental adecuada (λ = 3000 km en la primera octava)
  - Sin costuras de UV ni patrón de rejilla polar
  - Alturas consistentes tanto en tierra como en mar (mezcla continua océano/relieve)

- **Factor de desplazamiento:** ajustado de `h * 0.01f` a `h * 0.006f` donde `h` ahora está en km (antes era normalizado 0..1). El relieve máximo es comparable visualmente pero con mejor escala física.

**2. Nubes con variación vertical 3D:**

- **Antes:** Mapa de alpha 2D plano sobre esfera → nubes con aspecto "pintado" o plano.

- **Después:** Ruido `Noise3D()` sobre vector 3D `(x/TEX_W, y/TEX_H, sin(latitud))` → nubes con:
  - Mayor profundidad aparente
  - Borde más suave hacia los polos (sin la "cinta" de cobertura polar)
  - Estructura interna más compleja sin repetir patrón cuadrado

### 2.3 Arquitectura del relieve

```
Ruido 3D sobre dir unitario
      │
      ├──→ Desplazamiento radial de vértices (main3d.cpp:206)
      │       Factor: 0.006 → relieve ~540 km máximo
      │
      └──→ Altitud3D(): fBm 17 octavas
              • Octava 0: λ=3000 km (montañas continentales)
              • Octavas 1-4: λ=1500-375 km (features medianos)
              • Octavas 5-12: λ=93-7.3 km (relieve detallado)
              • Octavas 13-16: λ=3.6-0.45 km (relieve fino)
              • Octava 17: λ≈45 m (pedruscos)
              • NORMALIZACION=0.4220 → pico de 9 km
```

### 2.4 Estado actual y conocidas

**Mejoras comprobadas con el nuevo ruido 3D:**
- Los patrones "cuadrados" han desaparecido al eliminar la rejilla 2D UV
- Los contrastes de altura son perceptibles al volar sobre el planeta
- Las nubes tienen profundidad y no parecen una capa plana pintada

Siguiente paso: anclar el nivel del mar al campo de continentes en lugar de al signo de `h` (issue documentado en `TERRENO_3D.md`).

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