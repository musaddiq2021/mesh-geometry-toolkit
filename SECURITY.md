# Security policy

The CLI reads untrusted OBJ data without running commands, loading material files,
or making network requests. Paths are passed to filesystem APIs rather than a shell.

Default input limits are 32 MiB total, 64 KiB per line, 250,000 positions,
250,000 emitted triangles, 250,000 texture/normal records combined and 256 corners
per face. Limits are checked before the associated buffer grows. Library callers
can supply `ObjReadLimits` for trusted data. Volume analysis allows at most
1,000,000 triangle pairs by default; exceeding it yields unavailable volume while
area and bounds remain useful. These are work/size limits, not a process-wide
memory quota or time limit.

Conversion replaces the named output after complete staged writing and closing.
It rejects identical input/output files and filesystem aliases. Choose the output
path carefully: an existing output is intentionally replaced. The sibling temporary
file is created exclusively; write/flush/close errors occur before replacement,
and failures attempt to clean up the temporary file. Replacement semantics depend
on the filesystem and platform. The caller must control the destination
directory. No cross-platform atomicity, crash durability, or defense against an
attacker replacing parent directories is claimed.

Floating-point geometric checks are not exact predicates. Unsupported polygons,
nonmanifold/intersecting surfaces, multiple shells and unresolved numerical cases
are rejected or reported as unavailable. Inputs deliberately near tolerances may
be classified according to those approximate checks. Do not use the tool as a
certified geometry validator or run it with elevated privileges. Use OS resource
limits when processing large batches from untrusted sources.

No sanitizer execution or remote CI result is implied by this policy. See
[the audit](docs/AUDIT.md) for checks actually performed and their limits.

For a suspected vulnerability, avoid posting credentials, private files or a working
exploit in a public issue. Once the repository is hosted, use its private vulnerability
reporting feature if enabled; otherwise ask the maintainer for a private contact.
No private reporting address or response-time promise has been configured yet.
