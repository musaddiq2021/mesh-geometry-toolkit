# Mesh Geometry Toolkit

A C++20 command-line tool and library for inspecting and converting Wavefront OBJ meshes. It reports geometry and topology, and computes enclosed volume only for supported closed surfaces.

## Features

- Positions, positive/negative indices and common slash-separated OBJ face syntax.
- Checked triangulation of planar strictly convex polygons, preserving winding.
- Surface area, bounds, width/height/depth, counts and area-weighted normals.
- Boundary/nonmanifold edge detection, vertex links, orientation and components.
- Signed/enclosed volume with topology, intersection and work-budget checks.
- Triangulated OBJ conversion with staged output and same-file protection.
- Configurable input limits, source-line errors and automated regression tests.

## Requirements

CMake 3.20+, a C++20 compiler and a compatible build tool. The core uses the C++ standard library and native file APIs; it needs no downloaded dependencies or internet access to build or run.

Verified locally: 64-bit Windows, MinGW GCC 16.1.0 and CMake 3.31.6. GitHub Actions targets Ubuntu 24.04/GCC and Windows Server 2022/MSVC; those remote jobs have not run yet. Linux and MSVC compatibility remain unverified locally.

## Build

From the repository root, with CMake, MinGW GCC and mingw32-make on the build shell's PATH:

```powershell
cmake -S . -B build/windows-debug -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/windows-debug --parallel 2
ctest --test-dir build/windows-debug --output-on-failure --no-tests=error

cmake -S . -B build/windows-release -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build/windows-release --parallel 2
ctest --test-dir build/windows-release --output-on-failure --no-tests=error
```

GCC/Clang targets use C++20, `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`, and effective Release `-O2`. MSVC is configured with `/W4 /WX /permissive-`. Tests remain active in Release.

Portable CMake commands for Linux, pending platform verification:

```sh
cmake -S . -B build/linux-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux-release --parallel 2
ctest --test-dir build/linux-release --output-on-failure --no-tests=error
```

For multi-configuration generators, use `cmake --build build --config Release` and `ctest --test-dir build -C Release`. The optional Windows verification script accepts existing tool paths and also checks isolated executable deployment; see [WALKTHROUGH.md](docs/WALKTHROUGH.md).

## Usage

```text
mesh-tool inspect INPUT.obj
mesh-tool convert INPUT.obj OUTPUT.obj
mesh-tool --help
mesh-tool --version
```

`convert` intentionally replaces the specified output after complete staged writing. Input and output must be different files, including aliases. Texture coordinates, supplied normals, materials, groups and smoothing metadata are validated/ignored as documented, then discarded. Choose the output path carefully. No cross-platform atomicity or crash-durability guarantee is made.

Exit codes: `0` successful analysis/conversion, including a report with unavailable volume; `1` input, geometry or I/O error; `2` incorrect command syntax. Quote filenames containing spaces. The program passes paths to filesystem APIs, never a shell.

## Example

```powershell
.\build\windows-release\mesh-tool.exe inspect .\examples\cube.obj
.\build\windows-release\mesh-tool.exe convert .\examples\cube.obj .\build\cube-triangles.obj
```

The unit cube has 8 positions and 12 triangles after its six quads are triangulated. Expected results: surface area **6**, enclosed volume **1**, dimensions **1 x 1 x 1**. Other samples include a unit-axis tetrahedron (volume `1/6`), a nonconvex L prism (area 14, volume 3), and an open square at negative coordinates (area 6, unavailable volume).

## Tests

Run CTest using the build commands above. The suite has 65 unit groups and six CTest entries, covering analytical geometry, precision, invalid input, topology, stream failures, limits and real-file conversion. The CLI checks also cover existing-output replacement, same-file/hard-link rejection and temporary-file cleanup after replacement failure.

On Windows, `scripts/verify.ps1 -Clean` configures Debug and Release using installed tools, runs CTest with OS-only PATH, and checks a copied EXE without adjacent runtime DLLs. `build.ps1` is an independent direct-GCC fallback.

ASan/UBSan could not link with the installed MinGW toolchain because their runtime libraries are absent. With a compiler that supplies them:

```text
cmake -S . -B build/sanitize -DCMAKE_BUILD_TYPE=Debug -DMESH_SANITIZERS=address,undefined -DMESH_STATIC_MINGW_RUNTIME=OFF
cmake --build build/sanitize
ctest --test-dir build/sanitize --output-on-failure --no-tests=error
```

No remote CI or sanitizer success is claimed. See [AUDIT.md](docs/AUDIT.md) for actual verification and residual risks.

## Implementation

`Mesh` owns positions and zero-based triangle indices. Public geometry consumers validate before indexing. OBJ numeric parsing is locale-independent and requires finite, complete tokens. A face fan is emitted only after planarity and strict half-plane checks. Area uses half a cross-product magnitude; volume uses compensated signed tetrahedron contributions after eligibility checks.

Undirected edge incidence, directed winding counts and vertex-link graphs distinguish boundaries, orientation errors and nonmanifold vertices. Isolated positions are included in bounds but do not affect surface volume. The CMake library target is `mesh::geometry`.

Read [MATHEMATICS.md](docs/MATHEMATICS.md) for formulas/tolerances, [DESIGN.md](docs/DESIGN.md) for architecture/contracts, and [WALKTHROUGH.md](docs/WALKTHROUGH.md) for build and code examples.

## Limits and supported geometry

Defaults: 32 MiB of OBJ input, 64 KiB per line, 250,000 positions, 250,000 triangles, 250,000 combined texture/normal records and 256 corners per face. Library callers can pass `ObjReadLimits`. Volume checks permit at most 1,000,000 triangle pairs by default; a larger surface still has area/bounds but unavailable volume. These controls are not process-wide memory/time quotas.

Concave, crossing, collinear-corner and nonplanar polygon records are unsupported. Nonconvex solids built from supported faces are allowed. Volume supports one closed, consistently oriented manifold surface; multiple shells, unexpected contacts and uncertain numeric cases are unavailable. Geometric predicates use floating-point tolerances rather than exact arithmetic.

Forward references, freeform geometry, OBJ line continuations and vertex-color extensions are unsupported. Degenerate/duplicate triangles and nonfinite coordinates are rejected. Empty meshes have area zero, no bounds and unavailable volume. Windows command-line paths outside the active code page remain unverified.

Conversion requires a caller-controlled destination directory; it is not a filesystem sandbox. See [SECURITY.md](SECURITY.md) for the untrusted-input and file-writing policy.

## Windows deployment

Default GNU MinGW executables statically link GCC, libstdc++ and POSIX threading runtimes using `-static -static-libgcc -static-libstdc++`. Copy the Release EXE to compatible 64-bit Windows with its system UCRT; no GCC installation or runtime DLL copies are needed. System Windows/UCRT dependencies remain. Other compiler runtime policies are unchanged.

`MESH_STATIC_MINGW_RUNTIME=OFF` opts out for specialized builds, including MinGW sanitizers; those builds must arrange their runtime dependencies separately. Explorer can launch this console program, but its window closes at exit; PowerShell retains the output.

## License

Licensed under the [MIT License](LICENSE). Copyright (c) 2026 Musaddiq Ali.
