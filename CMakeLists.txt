cmake_minimum_required(VERSION 3.10)

project(orbus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)

# CONFIGURACIÓN MANUAL DE TU RAYLIB LOCAL
# ==========================================
# Definimos la ruta a tu carpeta de Raylib en el disco C:
set(RAYLIB_PATH "C:/raylib/raylib")

# Creamos un objetivo (target) importado para Raylib
add_library(raylib STATIC IMPORTED)

# Le decimos a CMake exactamente dónde están los archivos .h y el archivo de librería .a
# (En el instalador de Windows, ambos suelen estar dentro de la carpeta 'src')
set_target_properties(raylib PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_PATH}/src"
    IMPORTED_LOCATION "${RAYLIB_PATH}/src/libraylib.a"
)

set(SOURCES
    src/main.cpp
    src/physics/GravityEngine.cpp
    src/entities/Spaceship.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm gdi32 opengl32)
endif()