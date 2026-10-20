# Code and security review

## Scope

The review covered every production header/source, unit and CLI tests, CMake,
PowerShell scripts, sample files, documentation, ignore rules and CI configuration.
Development and review included AI assistance; verification statements below refer
to executed checks, not claims about authorship or universal correctness.

The workspace had no Git repository/history when reviewed. Build/tool distributions,
manual probes and private agent handover files were inspected separately and excluded
from the publication candidates. No remote repository, commit or upload was created.

## Changes from review

| Concern | Change | Evidence |
| --- | --- | --- |
| Unbounded OBJ growth | Configurable limits checked before line/count growth | Inclusive byte, line, position, attribute, corner and emitted-triangle boundary tests |
| Quadratic intersection work | Overflow-safe pair-count budget before traversal | Tetrahedron succeeds at six pairs, returns unavailable at five; area unchanged |
| Destination truncation on failed save | Exclusive sibling temporary file, complete write/flush/close, then rename | Existing-output replacement, same-file/alias rejection, failed-replacement preservation and temporary cleanup tests |
| Untrusted diagnostic text | Do not echo unknown OBJ tokens; escape terminal control characters in CLI errors | Record and filename control-sequence regressions |
| Stream format/EOF errors | Unformatted serialization and exception-aware bounded input | Width/fill/locale and all eight exception-mask combinations tested |
| Windows Ctrl-Z truncation | Binary input mode | Malformed trailing records are rejected in a real-file test |
| Buffered output errors | Flush/check successful CLI output | Delayed stdout failure was fault-injected during the earlier engineering audit |
| Test verification disabled by cache | Force tests ON, reject empty suites | Cached-OFF recovery tested during earlier audit |
| Windows runtime DLL dependency | Static GCC/libstdc++/POSIX linkage on GNU MinGW | PE import checks and isolated EXE execution with OS-only PATH |

Input/geometry formulas and topology algorithms were retained. Resource limits and
file-writing policy changed deliberately at the trust boundary. Public API comments
now describe units, supported inputs, exceptions, numerical policy and return values.

## Source review conclusions

Numeric parsing requires complete finite tokens. OBJ relative references use counts
at the face line, with separate position/texture/normal namespaces and safe handling
of the most-negative signed integer. Emitted triangles retain their source face line.

Convexity checks test every other corner against every edge, preventing a crossing
star from passing a consecutive-turn-only test. Area is half a cross-product magnitude.
Volume sums signed tetrahedra only after edge, winding, vertex-link, component,
intersection and work-budget checks. Vertex-link graphs catch pinches invisible to
edge incidence. Euler characteristic is reported, not used as a validity certificate.

Mesh ownership is by standard containers. Consumers validate indices before use;
stream views remain within their owners' lifetimes; staged file handles are released
on success/failure. Allocation failure is still possible within configured limits.
Native file APIs create staging files exclusively and handle short writes. Final
replacement assumes the caller controls the parent directory; no filesystem sandbox,
platform-independent atomicity or power-loss durability is promised.

## Tests performed

Locally verified toolchain: Windows x64, MinGW GCC 16.1.0, CMake/CTest 3.31.6.

- Fresh Debug and Release builds using README's PowerShell commands passed all six
  CTest entries, including 65 unit groups. All original 60 groups were retained.
- Clean automated Debug/Release verification additionally runs CTest with OS-only
  PATH and tests an isolated executable copy without compiler DLLs beside it.
- Analytical cases include cube area 6/volume 1, tetrahedron area
  `(3+sqrt(3))/2` and volume `1/6`, L prism area 14/volume 3, and a genus-one square
  ring area 48/volume 12. Open and negative-coordinate meshes retain correct area/bounds.
- Properties include 100 fixed-seed scaling/translations, 50 exact round trips of
  translated cubes and 30 rotations of the nonconvex prism. Tiny/large values use
  explicit relative-error checks; normal-array lengths prevent vacuous assertions.
- Invalid-input tests cover nonfinite values, oversized/signed/slash indices,
  duplicate/degenerate faces, unsupported polygons, source lines, EOF/read failures,
  resource limits and Windows text behavior.
- File tests cover conversion/reinspection, paths with spaces, intentional existing
  output replacement, input aliases, failed replacement and cleanup. Disk-full and
  power-loss behavior were not physically simulated; those error paths were reviewed.
- GCC `-fanalyzer` completed without diagnostics on the five production source files
  using strict C++20 warning flags. This is not proof of memory safety.
- Actionlint 1.7.12 passed workflow YAML, expressions and job configuration. Checkout
  is pinned to the verified v7.0.1 commit, permissions are read-only and credentials
  are not persisted. Jobs require no repository secrets and contain no installer scripts.

GitHub Actions has not run remotely. Ubuntu/GCC and Windows/MSVC jobs are configured,
but Linux, MSVC and other physical Windows machines remain unverified. ASan/UBSan
could not link in the installed MinGW toolchain because the runtime libraries are absent.
The optional Codex Security scan preflight was unavailable because Python was missing;
this report describes the local engineering review and does not claim a completed
plugin scan or independent-agent audit.

## Secret and privacy checks

Gitleaks 8.30.1 scanned the workspace and a separate copy of the publication candidates.
Its output template retains only file, rule and line, with secret values redacted.
The candidate scan reported no findings. Supplemental pattern checks and manual review
looked for personal paths/contact details, credential-bearing URLs, internal hosts,
environment files, private configuration and executable artifacts in the candidates.
No project credentials or personal data were detected by these checks.

The wider local scan flagged three excluded third-party items: a certificate container
in the local CMake distribution and two credential-like examples in the auditing tool's
own documentation. They were not copied into the publication set; validity was not
probed. Tool/build trees and agent history remain local. There was no existing project
Git history to scan and no tracked/indexed files requiring removal. Pattern scanning
is not a guarantee that all possible secrets or personal information were detected.

The 2026-10-21 post-license recheck covered 31 publication candidates. Gitleaks and
supplemental privacy checks reported no findings; candidate review found no private
agent files or build artifacts. Only LICENSE and documentation changed; hashes of
source, headers, tests, samples, build configuration and scripts were unchanged.

## Remaining limits and release decisions

- On 2026-10-21, the maintainer selected the [MIT License](../LICENSE) with
  Copyright (c) 2026 Musaddiq Ali. The licensing blocker is resolved.
- The workflow must execute on GitHub before remote CI success can be claimed.
- Geometry predicates use approximate tolerances. The tests do not certify every
  hostile/ill-conditioned mesh, self-intersection or branch of the implementation.
- Limits are not a process memory quota or deadline. Large inputs within those limits
  may still be expensive; use OS isolation/resource controls for untrusted batches.
- Multiple shells, concave polygon records and native Windows Unicode CLI paths beyond
  the active code page remain unsupported/unverified as stated in README.
- Destination parent directories must be trusted. Staged writing is not crash-durable
  and replacement behavior depends on platform/filesystem semantics.

See [SECURITY.md](../SECURITY.md) for policy and [PUBLICATION.md](PUBLICATION.md) for
the final local review and publication commands. No publication was performed.
