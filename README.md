# Mesh Geometry Toolkit

A C++20 command-line tool for reading, validating, and analysing 3D meshes stored in Wavefront OBJ files.

The toolkit implements computational geometry algorithms to calculate surface area, enclosed volume, and axis-aligned bounding boxes. It also supports polygon triangulation, mesh topology validation, and OBJ file conversion.

The project explores practical C++ development, numerical precision, geometric algorithms, and defensive software engineering without relying on external geometry libraries.

## Features

- **OBJ Parsing** — Reads vertex coordinates, face definitions, positive and negative indices, and common OBJ face formats.
- **Polygon Triangulation** — Converts supported planar, strictly convex polygons into triangles while preserving face orientation.
- **Surface Area** — Calculates the total surface area using vector cross products.
- **Enclosed Volume** — Computes signed tetrahedral contributions for supported closed, consistently oriented meshes.
- **Bounding Boxes** — Determines the minimum and maximum coordinates, width, height, and depth.
- **Topology Validation** — Identifies open boundaries, non-manifold edges, inconsistent winding, and unsupported geometry.
- **OBJ Conversion** — Exports triangulated OBJ geometry while protecting against accidental input-file replacement.
- **Input Safety** — Validates malformed records, numerical values, indices, and resource limits.

## Technologies

| Component | Technology |
|---|---|
| Programming Language | C++20 |
| Build System | CMake 3.20+ |
| Compilers | GCC / MSVC configuration |
| Testing | CTest and C++ unit tests |
| File Format | Wavefront OBJ |
| Development Environment | Windows, PowerShell |
| Continuous Integration | GitHub Actions |

The project uses the C++ standard library and does not require an external rendering engine or geometry-processing dependency.

Strict compiler warnings include:

```text
-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
```

## Getting Started

### Requirements

- CMake 3.20 or newer
- A C++20-compatible compiler
- A compatible build tool, such as MinGW Make
- Git (for cloning the repository)

### Clone the Repository

```bash
git clone https://github.com/musaddiq2021/mesh-geometry-toolkit.git
cd mesh-geometry-toolkit
```

### Build on Windows

For MinGW GCC and `mingw32-make`, open PowerShell in the repository directory:

```powershell
cmake -S . -B build/windows-release -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

cmake --build build/windows-release --parallel 2
```

The compiled application will be available at:

```text
build/windows-release/mesh-tool.exe
```

To build the Debug configuration:

```powershell
cmake -S . -B build/windows-debug -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug

cmake --build build/windows-debug --parallel 2
```

### Build on Linux

With a compatible C++20 toolchain:

```bash
cmake -S . -B build/linux-release -DCMAKE_BUILD_TYPE=Release

cmake --build build/linux-release --parallel 2
```

Linux builds are configured through GitHub Actions; platform compatibility depends on the available compiler and environment.

## Usage

The application provides two primary operations: inspecting and converting OBJ files.

### Inspect a 3D Mesh

```powershell
.\build\windows-release\mesh-tool.exe inspect .\examples\cube.obj
```

The program reads the mesh, validates its geometry, and calculates its properties.

For the included unit cube, the expected results are:

| Property | Value |
|---|---:|
| Vertices | 8 |
| Triangles | 12 |
| Surface Area | 6 |
| Enclosed Volume | 1 |
| Bounding Dimensions | 1 × 1 × 1 |

Surface area is expressed in squared coordinate units, while volume is expressed in cubed coordinate units.

### Convert an OBJ File

```powershell
.\build\windows-release\mesh-tool.exe convert .\examples\cube.obj .\build\cube-triangulated.obj
```

This produces an OBJ file containing vertex positions and triangulated faces.

The converted file can be inspected using:

```powershell
.\build\windows-release\mesh-tool.exe inspect .\build\cube-triangulated.obj
```

The conversion process preserves the supported geometry but does not retain all original OBJ metadata, such as materials, texture coordinates, and smoothing groups.

### Available Commands

```text
mesh-tool inspect INPUT.obj
mesh-tool convert INPUT.obj OUTPUT.obj
mesh-tool --help
mesh-tool --version
```

## How It Works

### Mesh Representation

A 3D mesh consists of vertices, edges, and faces.

Vertices define positions in three-dimensional space. Faces reference these positions to construct the surface of an object.

The toolkit stores vertex positions and indexed triangles, allowing geometric calculations without repeatedly duplicating vertex data.

### Surface Area

Triangle area is calculated using the cross product of two vectors:

```text
Area = 0.5 × |(B − A) × (C − A)|
```

Where A, B, and C represent the triangle's vertices.

The total mesh surface area is the sum of the individual triangle areas.

This method works for triangles in arbitrary 3D orientations.

### Enclosed Volume

The toolkit uses signed tetrahedral volume calculations:

```text
V = (1/6) × Σ dot(A, cross(B, C))
```

The algorithm accumulates signed contributions from the oriented surface triangles, using numerical techniques to reduce floating-point error.

Enclosed volume is reported only when the mesh satisfies the supported topology and geometry requirements.

Open meshes or unsupported configurations return an unavailable volume rather than an unreliable result.

### Topology Validation

The toolkit examines how triangles connect through shared edges.

For a consistently oriented, closed manifold surface, each edge belongs to exactly two triangles that traverse the shared edge in opposite directions.

Validation identifies boundary edges, inconsistent face orientations, non-manifold configurations, and other unsupported geometric conditions.

### Axis-Aligned Bounding Box

The bounding box is calculated using the minimum and maximum vertex coordinates along each axis.

```text
Minimum = (minX, minY, minZ)
Maximum = (maxX, maxY, maxZ)

Dimensions = Maximum − Minimum
```

This produces a rectangular bounding box aligned with the coordinate axes.

## Testing and Verification

The project includes automated tests covering geometric calculations, OBJ parsing, numerical precision, topology validation, command-line behaviour, and file handling.

Test cases include:

- Cubes and tetrahedra with known mathematical results
- Open and incorrectly oriented meshes
- Nonconvex solids built from supported faces
- Negative coordinates and translated geometry
- Extreme-scale floating-point calculations
- Invalid OBJ syntax and vertex indices
- Degenerate and unsupported polygons
- OBJ conversion and reinspection
- Input resource limits and file-stream failures

### Run Automated Tests

On Windows:

```powershell
.\scripts\verify.ps1 -Clean
```

Alternatively, run CTest against an existing build:

```powershell
ctest --test-dir build/windows-release --output-on-failure --no-tests=error
```

### Local Verification Results

| Check | Result |
|---|---|
| Automated Unit Test Groups | 65 passed |
| CTest Entries | 6/6 per configuration |
| Debug Build | Passed |
| Release Build | Passed |
| Strict Compiler Warnings | Passed |
| GCC Static Analysis | No diagnostics reported |
| Standalone Windows Executable | Verified locally |

The results above reflect the local engineering audit. Sanitizer testing and additional compiler/platform combinations remain subject to toolchain availability and further verification.

## Project Structure

```text
mesh-geometry-toolkit/
│
├── .github/
│   └── workflows/
│       └── ci.yml
│
├── include/
│   └── mesh/
│
├── src/
│
├── tests/
│
├── examples/
│
├── scripts/
│
├── docs/
│
├── CMakeLists.txt
├── build.ps1
├── README.md
├── SECURITY.md
└── LICENSE
```

The architecture separates OBJ parsing, polygon triangulation, geometric calculations, validation, and command-line functionality.

The geometry library is also exposed as the CMake target `mesh::geometry`.

## Limitations

The toolkit focuses on supported OBJ mesh analysis rather than general-purpose CAD modelling.

Current limitations include:

- Concave, crossing, and nonplanar polygon face records are not supported.
- Enclosed-volume analysis is restricted to supported closed, consistently oriented manifold surfaces.
- Multiple-shell geometry and some complex geometric intersections are unsupported.
- Numerical decisions rely on floating-point tolerances rather than exact geometric predicates.
- Resource limits restrict excessively large inputs and expensive intersection calculations.
- OBJ conversion preserves supported geometry, not all original OBJ metadata.
- Staged file output does not guarantee crash durability or filesystem atomicity.

These constraints help distinguish reliable supported behaviour from geometry requiring more advanced algorithms.

## Documentation

Further technical information is available in the repository:

- [Mathematics](docs/MATHEMATICS.md) — Vector operations, surface area, volume calculations, and numerical precision.
- [Design](docs/DESIGN.md) — Software architecture and implementation decisions.
- [Walkthrough](docs/WALKTHROUGH.md) — Code structure, build instructions, and execution flow.
- [Audit](docs/AUDIT.md) — Engineering review, regression testing, and known limitations.
- [Security](SECURITY.md) — Input validation, resource safety, and security considerations.

## Project Motivation

This project was developed to explore how mathematical algorithms can be implemented, validated, and tested in modern C++.

Rather than relying on existing geometry-processing libraries, it focuses on understanding the underlying calculations and the practical engineering challenges involved in processing 3D data.

The implementation brings together computational geometry, indexed data structures, floating-point arithmetic, file parsing, error handling, automated testing, and CMake-based build workflows.

It serves as a practical exploration of C++20 and the relationship between mathematical correctness and reliable software implementation.

## License

Licensed under the [MIT License](https://github.com/musaddiq2021/mesh-geometry-toolkit/blob/main/LICENSE). Copyright (c) 2026 Musaddiq Ali.
