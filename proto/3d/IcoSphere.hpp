#pragma once
#include <vector>

struct IcoVertex {
    float x, y, z;
};

// Genera la icosfera de profundidad `depth` (vértices sobre radio 1 e índices de triángulos).
// Cada una de las 20 caras base ocupa 4^depth triángulos consecutivos en `indices`.
void BuildIcosphere(int depth, std::vector<IcoVertex>& vertices, std::vector<int>& indices);
