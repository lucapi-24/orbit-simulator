# Icosphere Implementation Notes

## Why Icosphere vs GenMeshSphere

The original `GenMeshSphere(1.0f, 256, 128)` creates a mesh with **rectangular grid topology**:
- 256 divisions around the equator (longitude)
- 128 divisions from pole to pole (latitude)

**Problem:** Each grid cell maps to ~1484 km at planet scale, and the grid lines become visible as "squared" patterns on the terrain. The 2D noise grids (`gA`, `gB`, etc.) align exactly with these rows/columns, creating repetitive patterns.

**Icosphere solution:** Starting from an icosahedron (20 triangular faces) and subdividing recursively produces:
- All faces are equilateral triangles
- No orthogonal lines/rows
- Vertices distributed more naturally on the sphere
- When noise is evaluated on vertex directions (3D), the terrain appears organic

## Implementation Details

### Function: `IcoSubdivideTriangle`

```cpp
static void IcoSubdivideTriangle(
    const IcoVertex& a, const IcoVertex& b, const IcoVertex& c,
    int depth, std::vector<IcoVertex>& vertices, std::vector<int>& indices
)
```

- **Depth 3** = 642 vertices, good balance of detail vs performance
- Each subdivision divides a triangle into 4 smaller triangles
- Midpoint vertices are averaged and normalized (keeps them on the sphere surface)
- Recursively called until `depth <= 0`, then base triangle is added

### Icosahedron topology

- **12 vertices** at golden ratio coordinates `(±1, ±φ, 0)` and permutations
- **20 triangular faces** defined by `TRIANGLES_ICOSAEDRO[20][3]`
- All edges are equivalent (regular icosahedron)

### UV generation (spherical)

For each vertex, UVs are computed from spherical coordinates:
```cpp
float theta = atan2f(v.z, v.x) / (2.0f * PI) + 0.5f;  // 0..1 longitude
float phi   = acosf(v.y / 1.0f) / PI;                 // 0..1 latitude (north=0)
```
- `theta` maps longitude around the sphere
- `phi` maps latitude from pole to pole
- This gives a consistent mapping independent of any rectangular grid

### Normal generation

Since the Icosphere vertices are already normalized (length = 1), the normals are simply the vertex coordinates themselves. No additional normal calculation needed - this gives smooth shading for radial displacement.

## Changes in `main3d.cpp`

1. **Added** at the top of the file:
   - `IcoVertex` struct
   - `IcoVertexMake`, `IcoVertexLen`, `IcoVertexNorm` helpers
   - `PHI` constant (golden ratio)
   - `VERTICES_ICOSAEDRO[12]` - 12 vertex positions
   - `TRIANGLES_ICOSAEDRO[20][3]` - 20 face indices
   - `IcoSubdivideTriangle()` function (recursive subdivision)

2. **Replaced** `Mesh sphereMesh = GenMeshSphere(1.0f, 256, 128);` with:
   - Generate Icosphere via `IcoSubdivideTriangle`
   - Convert to `Mesh` raylib format
   - Calculate spherical UVs per vertex
   - Set up vertex buffers

3. **Texture mapping** uses the spherical UVs calculated above instead of the transposed UVs from `GenMeshSphere`.

## Expected Visual Result

| Feature | GenMeshSphere (old) | Icosphere (new) |
|---------|---------------------|------------------|
| Grid pattern visible | Yes (at all distances) | No, eliminated |
| Terrain appearance | Rectangular cells | Organic, natural |
| Close-up detail | Repeating square patterns | Smooth triangles |
| Performance | 31,808 vertices | 642 vertices (62x fewer!) |
| Relief perception | Okay, but grid-like | Better, more natural |

## Performance Impact

- **Vertices:** 31,808 → 642 ( ~98% reduction)
- **Triangles:** 61,808 → 1,926 ( ~97% reduction)
- **GPU memory:** Significant reduction
- **Draw call:** Same (one mesh, one draw)

The fewer vertices actually improves performance, and the visual quality is higher due to the organic topology.

## Next Steps (Optional)

If further improvements are desired:
1. Add **tessellation shader** for LOD (level of detail)
2. Add **normal mapping** for finer detail without extra geometry
3. Increase Icosphere subdivision depth (4 = 3866 vertices, 5 = 24562)