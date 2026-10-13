@echo off
:: Orbit Simulator - Build both orbus and proto3d
:: Usage: build_all.bat

set "RAYLIB_PATH=C:/raylib/raylib"

mkdir -p build
cd build

if not exist CMakeCache.txt (
    echo === Configurando cmake ===
    cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DRAYLIB_PATH=%RAYLIB_PATH%
)

echo === Compilando orbus.exe y proto3d ===
cmake --build . --config Release --target orbus --target proto3d

echo.
echo === Build completo ===
echo orbus.exe listo
echo proto3d.exe listo

echo.
