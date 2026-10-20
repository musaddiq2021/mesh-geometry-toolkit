# Architecture and engineering decisions

## Data flow

```text
OBJ file/stream
  -> numeric/reference parser
  -> checked face triangulation
  -> structural mesh validation
  -> Mesh (owned positions + zero-based triangles)
  -> bounds / area / normals / topology / volume eligibility
  -> CLI report or triangulated OBJ output
```

The library owns geometry and diagnostics. The CLI owns command syntax and presentation. No third-party runtime libraries, GUI, global state, external material loading, or automatic mesh repair are needed.

## Files and responsibilities

| Files | Responsibility |
| --- | --- |
| `include/mesh/geometry.hpp` | Vec3 arithmetic, Triangle/Mesh value types, validation, Bounds, Topology, VolumeAnalysis API |
| `src/geometry.cpp` | Validation, AABB, area/normals, edge incidence, disjoint sets, vertex-link graphs |
| `include/mesh/triangulation.hpp`, `src/triangulation.cpp` | Checked planar strictly convex fan triangulation |
| `include/mesh/obj.hpp`, `src/obj.cpp` | OBJ tokens/references, source diagnostics, stream/file load/save |
| `src/volume.cpp` | Eligibility, triangle contact checks, compensated tetrahedron contributions |
| `src/main.cpp` | inspect/convert/help/version and exit codes |
| `tests/test_main.cpp` | Dependency-free test registry and analytical/property/regression tests |
| `tests/cli.cmake` | Portable real-process/file CLI acceptance checks |
| `tests/fixtures/` | Malformed OBJ inputs |
| `examples/` | Cube, tetrahedron, negative-coordinate open square, inconsistent cube, nonconvex L prism |
| `CMakeLists.txt` | Strict C++20 library, CLI, tests and optional sanitizer flags |
| `build.ps1`, `scripts/verify.ps1` | Existing GCC fallback and local CMake verification automation |
| `scripts/verify-windows-runtime.ps1` | PE import checks and isolated-copy execution with OS-only PATH |
| `docs/` | Mathematics, design and walkthrough |
| `SECURITY.md` | Input limits, output policy and residual security risks |

`build/` and local tool/editor state are excluded from source control. Private handover notes and manual probes are kept outside the publishable file set.

## Types and ownership

`Vec3` is three doubles. `Triangle` is an array of three `size_t` indices. `Mesh` owns vectors of both; no cross-object raw pointers or manual memory management are used. Input order is retained; a polygon expands into deterministic fan triangles. The mesh stores neither original polygon boundaries nor texture/material attributes.

Mesh data remains publicly editable for simple composition. Every public geometry consumer validates before indexing. Validation is intentionally repeated between independent operations rather than cached, because a cached result could be invalidated by direct edits. A future immutable validated-mesh view could reduce repeated work without stale-cache risks.

`std::optional<Bounds>` distinguishes empty geometry from a box at the origin. `VolumeAnalysis` has optional signed/enclosed values and a reason. Unsupported topology/geometry returns no volume; a fabricated zero never stands in for failure. `signed_volume()` is the throwing convenience API. Invalid mesh structure throws `std::invalid_argument`; invalid OBJ records throw `ObjError` with source line; I/O failures throw runtime errors. Primitive vector operations have narrower documented contracts.

## Parsing policy

The parser tokenizes one line at a time, removes comments, and uses locale-independent `from_chars` for numeric conversion. Integer arithmetic avoids negating `INT64_MIN`. Positive OBJ indices are one-based; negative indices count backward from elements already defined at that face line. Position, texture and normal references have independent counts. All slash suffixes are validated before attributes are discarded.

Input must initially be in a good state. The line reader distinguishes ordinary EOF from actual read failures even when the caller enables stream exceptions; a final line without a newline is processed once. The exception mask is never changed. Output uses unformatted `write` from the private serialization buffer so width, fill, alignment and locale cannot pad or corrupt OBJ records. Caller formatting remains intact.

Each emitted triangle records its source face line while parsing. Post-parse duplicate/geometry errors can therefore report the originating OBJ line after a polygon expanded into several triangles. Nonfinite positions are rejected immediately, including homogeneous division overflow. Unknown geometric records fail explicitly; only documented metadata is ignored.

The parser checks convexity before using a fan. It does not try to repair polygons or guess vertex order. That makes unsupported input predictable. ObjReadLimits bounds input bytes, line length, positions, attributes, corners and emitted triangles before growth. Allocation can still fail within those limits; it propagates to the CLI error path. Library callers processing already-constructed meshes must provide their own memory policy.

## Supported volume boundary

Topology builds undirected edge incidences and vertex-link graphs. Intersection checks then validate that triangle contacts match shared mesh indices. A single nonconvex closed surface is supported; the analytical L prism demonstrates this. Multiple surface components are rejected before volume summation. A configurable triangle-pair budget also prevents unbounded quadratic work; exceeding it reports unavailable volume without changing area or bounds.

Floating-point intersection tests are conservative within a documented numeric policy, rather than a general exact geometry kernel. `closed_consistently_oriented` reports topology only; it does not promise volume eligibility. Call `analyze_volume()` for the final decision. Surface area and bounds remain useful for structurally valid open, nonmanifold, or intersecting meshes.

## Complexity and tradeoffs

Let `V` be positions, `F` triangles and `n` corners of an imported polygon.

- Structural validation uses a sorted-triangle set: `O(V + F log F)` time and `O(F)` auxiliary storage.
- Polygon checking is `O(n²)` time and `O(n)` local storage before emitting `n-2` triangles.
- Bounds, area and normals perform linear geometry work after validation.
- Edge topology uses ordered maps and vertex-link traversal: approximately `O(V + F log F)` time and `O(V + F)` storage.
- Volume checks at most `F(F-1)/2` triangle pairs, followed by linear accumulation. It uses `O(V + F)` topology storage and constant-size per-pair contact storage.
- OBJ writing stages serialized output in memory to preserve caller stream state and avoid writing invalid geometry; memory includes the serialized file size. File save serializes before creating an exclusively reserved sibling temporary file, handles partial writes, flushes/closes it, then replaces the destination with filesystem rename. Failure attempts temporary cleanup; no cross-platform atomicity or crash-durability promise is made.

Ordered containers favor deterministic diagnostics and straightforward review. A spatial index could accelerate pair checks, and immutable validation could avoid repeated sorting, but neither is required for this modest standalone toolkit. Adding those optimizations requires measurements and equivalent correctness tests.

## Build and verification design

The static library is available through `mesh::geometry`. Tests do not use `assert`, so they remain effective with `NDEBUG`. No network downloads occur during CMake configuration. GCC/Clang builds use strict warnings including `-Werror` and effective Release `-O2`; MSVC uses `/W4 /WX`. `BUILD_TESTING=OFF` excludes tests. `MESH_SANITIZERS` adds compiler/link flags only when explicitly enabled on a compatible toolchain.

For MinGW GNU C++, default `MESH_STATIC_MINGW_RUNTIME=ON` adds `-static -static-libgcc -static-libstdc++` to executable links. Full static archive selection includes POSIX threading; Windows/UCRT imports remain supplied by the operating system. The verification runner rejects unexpected PE imports and tests a copied EXE without adjacent DLLs or GCC PATH. Sanitizer builds must explicitly disable this option; non-MinGW/non-GNU compiler runtime policies are unchanged.

The CTest acceptance script uses actual subprocess exit codes and reads real converted files. Metric assertions match entire lines, avoiding false acceptance of volume 10 for expected 1. It checks paths containing spaces, destination preservation for invalid input, and missing output directories. The verification script forces `BUILD_TESTING=ON` and passes `--no-tests=error` so a cached test-disabled configuration cannot claim verification. Unit tests cover analytic shapes, fixed-seed invariants, malformed streams and exact floating-point round trips. Tiny/large analytical values use explicit relative error checks; normal-array lengths are asserted to avoid vacuous loops. Debug enables available standard-library assertions. Sanitizer runtime availability is reported separately from successful unsanitized testing.

## Extension discipline

Keep successful behavior and extend one subsystem with focused tests. Possible future work includes exact predicates, broad-phase acceleration, richer OBJ metadata, shell nesting analysis, configurable tolerances, and Windows Unicode argument handling. Each changes a contract and should be designed explicitly rather than silently widening support.

Interview prompts: Why validate public mesh data on every call? What invalidates a cached topology result? Why separate unsupported volume from malformed input? How would a BVH change complexity without weakening collision tests?

Exercises: sketch an immutable validated mesh interface; explain which staged-save guarantees fail if another process controls the destination directory; add a new analytical mesh test before optimizing the intersection loop; propose a nesting policy for multi-shell solids and list the extra tests needed.
