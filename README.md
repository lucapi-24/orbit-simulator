# Orbus — Orbital Simulator

An educational sandbox with two parallel tracks:

- **2D** — the orbital simulator (Python prototype + C++ game).
- **3D** — a procedural-planet sandbox, currently in development.

Both share the same raylib build. The C++ game and the 3D sandbox are separate executables.

---

## Requirements

| Dependency | Notes |
|---|---|
| Windows + MSYS2 UCRT64 | Toolchain: `g++`, `mingw32-make` (at `C:\msys64\ucrt64\bin`) |
| CMake ≥ 3.10 | |
| raylib (local build) | Expected at `C:/raylib/raylib`, static lib at `src/libraylib.a` |
| Python 3 venv | `numpy`, `matplotlib` — already in `.venv/` |

---

## Part 1 — 2D Orbital Simulator

### 1.1 C++ game

```powershell
# first time only
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build

.\build\orbus.exe
```

**Controls**

| Input | Action |
|---|---|
| `W` / `A` / `S` / `D` | Rotate and thrust |
| Arrow keys | Same as WASD |
| Mouse wheel | Zoom |
| `,` / `.` | Time warp down / up |
| `C` | Cycle camera-follow target |
| `V` | Free-pan camera (then `WASD` pans) |
| GUI button | Hohmann transfer to the Moon, auto-circularizes |

**Physics**

- N-body gravity through one unified `GravityEngine::ComputeAcceleration(pos, bodies)`, `const`, double internally / float at the boundary.
- Plummer softening, ε² = 25: `a = G·M·r_vec/(r² + ε²)^{3/2}`. Free self-exclusion (`r → 0` ⇒ contribution 0).
- Velocity Verlet. All positions are advanced before accelerations are recomputed (N-body consistency).
- Fixed timestep accumulator: `FIXED_DT = 1/240`, `MAX_PASOS = 5`, real-time cap 0.1 s. Physics (including input) runs in the fixed step; camera, prediction and drawing run once per frame.
- The Sun is static (`CelestialBody::isStatic`) — it exerts gravity but is never integrated.

### 1.2 Python prototype

```powershell
.\.venv\Scripts\Activate.ps1
python main.py
```

**Controls:** `space` = Hohmann, `↑` / `↓` = prograde / retrograde, `,` / `.` = time speed.

Earth–Moon system, RK4 integration, orbital telemetry. This is where the physics ideas are validated before being ported to C++.

---

## Part 2 — 3D Procedural Planet Sandbox

### 2.1 Build and run

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release   # first time
cmake --build build --target proto3d
.\build\proto3d.exe
```

> **Run it from the repository root.** Shaders are loaded with relative paths
> (`proto/3d/shaders/...`), so the working directory must be the repo root.

**Controls**

| Input | Action |
|---|---|
| `W` `A` `S` `D` | Move (forward / left / back / right) |
| `Q` / `E` | Down / up |
| Mouse wheel | Move speed |
| `F` | Teleport to the surface and look at the horizon (ray-triangle against the real mesh) |

### 2.2 Noise validation test (no window)

Writes PNGs to `proto/3d/out/` and prints measurements to the console.

```powershell
cmake --build build --target noise_test
.\build\noise_test.exe
```

Or compile it directly, skipping CMake:

```powershell
$env:PATH="C:\msys64\ucrt64\bin;$env:PATH"
g++ -std=c++17 -O2 -I "C:/raylib/raylib/src" -I proto/3d `
    proto/3d/noise_test.cpp -o build/noise_test.exe `
    -L"C:/raylib/raylib/src" -lraylib -lopengl32 -lgdi32 -lwinmm
.\build\noise_test.exe
```

Run it from the repo root — it writes to `proto/3d/out/`.

**What it measures:** 1D profile, 512×256 equirectangular map, continent field, continuity (seam / pole / slope), and a calibration sweep.

### 2.3 Architecture

```
proto/3d/
  main3d.cpp       # 3D sandbox: FPS camera, planet, three shells, custom shaders
  Noise3D.hpp      # standalone 3D gradient noise (not yet wired into the planet)
  noise_test.cpp   # headless validation of the noise model
  TERRENO_3D.md    # design document for the 3D terrain
  shaders/         # terreno.{vs,fs}, nubes.{vs,fs}, atmosfera.{vs,fs}
  out/             # generated PNGs (gitignored)
```

**`Noise3D.hpp`**

- Units: `1 unit = 1 km`, planet radius `R = 6000 km`.
- Integer hash, 12 gradients along cube edges, quintic fade.
- `Altitud3DEx(dir, contBias, norm)` → altitude in km above sea level; `Altitud3D(dir)` is the fixed-argument shortcut used by the game.
- Continental field: `Cont3DEx(dir, contBias)` (the only place the math lives) and `Cont3D(dir)` (`contBias = CONT_BIAS`). `Altitud3DEx` consumes `Cont3DEx`.
- 17-octave auto-affine fBm: `FREC_BASE = 2`, `FBM_LACUN = 2`, `FBM_GAIN = 0.5`. Frequency `f` maps to wavelength `R/f`, so the highest octave is ~45 m.
- The scale factors are calibrated: `NORMALIZACION = 0.4220`, `CONT_BIAS = 0.25`. Changing `FBM_OCTAVES`, `AMP_BASE` or `FREC_BASE` requires re-running the sweep.

**Terrain pipeline (current)**

```
dir ─┬─► Cont3DEx ──► cont ──► mask = SmoothStep(0.10, 0.35, cont)
     └─► fBm(17 oct) ─► h (signed, mean 0, ±9 km)
                                  ▼
              ocean * (1 - mask) + h * mask
```

### 2.4 Current status and known issues

Step 0 (measure the continent field) is **done**. Findings:

- The coast belongs at `cont > +0.395` — that gives **29.7% of real surface** (target: 29%).
- The field yields **three real continents** (9.6%, 9.5% and 6.4% of the planet) plus 7 small islands. Sizes match Earth's continents, so the continent field itself is fine.
- North/south land split is 31.0% / 30.5% — perfectly isotropic, as expected. Real Earth is ~65/35; fixing that needs a latitude-dependent term.
- The transition band 0.35–0.39 covers only 4.8% of the area (a shoreline), versus the current `mask` band 0.10–0.35 covering ~30% (a third of the planet stuck in blending).

Known issues, documented in `TERRENO_3D.md`:

1. **Sea level is decided by the sign of `h`.** On land `mask = 1`, so elevation is just `h`, which is signed — roughly half of every continent is submerged.
2. **The ocean term can be positive** (`+1.03 km`), because signed noise is added to an absolute position.
3. **`FBM_GAIN = 0.5` gives equal slope per octave**, which reads as noise rather than relief.
4. **No LOD** — 17 octaves everywhere. The global mesh only supports ~4, a texture ~7, and a 5 km chunk at 256² can justify all 17.

Next step (2): anchor the sea level to `cont` instead of to the sign of `h`.

### 2.5 Measurement gotcha

`noise_test` reports **pixel** fractions, not area fractions. On an equirectangular map a pixel's area scales with `cos(lat)`, and land is concentrated near the equator, so pixel counts under-report by **~2.4 percentage points**. `TestMapa` and `TestCalibracion` currently calibrate against pixels; they should be area-weighted before the next calibration pass.

---

## Project structure

```
include/
  physics/PhysicalState.hpp   # {position, velocity, mass} (raylib Vector2)
  physics/GravityEngine.hpp   # ComputeAcceleration + integrators + PredictTrajectories
  entities/CelestialBody.hpp  # base body (+ isStatic) + Draw()
  entities/Spaceship.hpp      # extends CelestialBody; fuel, angle, thrust
src/
  main.cpp                    # game loop: accumulator → fixed steps → prediction → draw
  physics/GravityEngine.cpp
  entities/Spaceship.cpp
proto/3d/                     # 3D sandbox (see Part 2)
main.py                       # Python 2D prototype
pyhisics.py                   # physics core for the prototype
```

---

## Conventions

- Comments and identifiers mix Spanish and English (physics in Spanish, entities in English).
- No test framework: verification is by running the binaries plus a compilation check.
- `build/`, `.venv/` and `proto/3d/out/` are gitignored — never commit generated artifacts.
