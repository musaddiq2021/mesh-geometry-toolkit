# Build and code walkthrough

## Build on Windows

Use a PowerShell session with CMake and the MinGW GCC toolchain on PATH. No build
step downloads dependencies. From the repository root:

```powershell
cmake -S . -B build/windows-release -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release
cmake --build build/windows-release --parallel 2
ctest --test-dir build/windows-release --output-on-failure --no-tests=error
.\build\windows-release\mesh-tool.exe inspect .\examples\cube.obj
```

Replace `windows-release`/`Release` with `windows-debug`/`Debug` for a separate debug
build. Use `--clean-first` to rebuild all targets. Do not mix generators in one build
directory. README's equivalent Linux commands are pending platform verification.

The optional `scripts/verify.ps1 -Clean` runs both MinGW configurations, all CTest
entries with OS-only PATH, and an isolated-copy runtime check. Supply `-CMake`,
`-Compiler` and `-Make` if those tools are not discoverable on PATH. The script
restores process environment changes and keeps generated files under `build/`.
`build.ps1` provides an independent direct-GCC build/test path.

Default MinGW executables embed GCC/libstdc++/POSIX threading runtimes. The Release
EXE runs without GCC on a compatible x64 Windows system with system UCRT. Repeat the
standalone check with `scripts/verify-windows-runtime.ps1 -Executable PATH_TO_EXE`;
that inspection step needs `objdump` on PATH or its `-ObjDump` argument.

## Check the samples

The cube reports area 6, enclosed volume 1, dimensions 1/1/1, eight positions and
12 triangles. It has 18 triangle edges because six face diagonals count as edges.
The tetrahedron gives area approximately 2.3660254037844384 and volume 1/6.
`open_square.obj` has negative-coordinate bounds and area 6 but unavailable volume.
`inconsistent_cube.obj` retains area 6 while its winding prevents volume.
`concave_solid.obj` is an L prism with supported triangle/quad faces: area 14,
volume 3. Concave polygon records themselves are not supported.

```powershell
.\build\windows-release\mesh-tool.exe inspect .\examples\tetrahedron.obj
.\build\windows-release\mesh-tool.exe inspect .\examples\open_square.obj
.\build\windows-release\mesh-tool.exe inspect .\examples\inconsistent_cube.obj
.\build\windows-release\mesh-tool.exe convert .\examples\cube.obj .\build\triangles.obj
.\build\windows-release\mesh-tool.exe inspect .\build\triangles.obj
```

Conversion intentionally replaces a different output file only after staged writing
succeeds. Using the same file as input/output, including a hard-link alias, is an
error. For malformed inputs, try `tests/fixtures/invalid.obj` and `concave.obj`.
An unavailable volume is a successful analysis (exit 0), while malformed input is
exit 1 and incorrect command syntax is exit 2.

## Follow a face through the code

In `examples/cube.obj`, `f 1 4 3 2` resolves to zero-based positions in `src/obj.cpp`.
`triangulate_convex_polygon` checks planarity and every edge half-plane, then emits
`(0,3,2)` and `(0,2,1)`. Both output triangles retain the face's source line for later
diagnostics. Read limits bound bytes, line length, counts and corner work before
allocations grow.

In `src/geometry.cpp`, area uses cross products, topology canonicalizes edges, and
vertex links catch pinches invisible to edge incidence alone. `src/volume.cpp`
checks eligibility and the pair budget, tests triangle contacts, then sums signed
tetrahedra. `src/main.cpp` handles command syntax and report formatting.

## Tests and debugging

Run `mesh-tests` directly for individual group names. CTest logs are in each build's
`Testing/Temporary/LastTest.log`. `compile_commands.json` records standard and
warning flags. Tests use explicit checks rather than assertions disabled by NDEBUG.

A rotation regression protects a former false intersection caused by division by a
roundoff-sized coplanar edge determinant. Stream tests protect EOF exception-mask
handling and caller formatting. File tests protect alias rejection, preserved
output on failure and temporary cleanup. These examples explain why analytic
axis-aligned cubes alone are not sufficient coverage.

Sanitizers were unavailable in the locally installed MinGW toolchain; README shows
an opt-in configuration for a compiler with supported runtimes. GCC static analysis
is additional evidence, not a replacement for sanitizers.

## Questions and exercises

- Why do negative OBJ references depend on the face line rather than the final
  position count? Why is a zero OBJ index invalid?
- Why can a nonconvex solid be supported while a concave polygon record is rejected?
- Why does closed edge incidence fail to detect two tetrahedra pinched at a vertex?
- Why is an absolute signed sum insufficient for overlapping or nested shells?
- Copy the cube into `build/`, double all coordinates, and predict area 24/volume 8.
- Reverse every face and compare signed versus enclosed volume. Then reverse one
  face and explain the unavailable result.
- Add an unused position far away and predict changes in bounds, Euler
  characteristic and volume before running the tool.
- Lower `ObjReadLimits` in a small library program and verify both inclusive
  boundary acceptance and rejection of the next record.
