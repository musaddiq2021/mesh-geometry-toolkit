# Geometry and numerical policy

## Vectors and triangles

For positions `a`, `b`, `c`, the directed edge vectors are `u = b - a` and `v = c - a`. Their cross product is perpendicular to the triangle:

```text
u × v = (uy*vz - uz*vy, uz*vx - ux*vz, ux*vy - uy*vx)
triangle area = |u × v| / 2
unit face normal = (u × v) / |u × v|
```

Reversing two indices negates the normal and preserves area. Summing all triangle areas gives surface area, including for open meshes. The implementation uses `std::hypot` for vector length and Neumaier compensated summation for area to reduce loss from adding very different magnitudes.

Vertex normals accumulate the cross product from each incident triangle, then normalize the result. Since cross-product magnitude is twice area, this weights large faces more heavily. Isolated vertices and exactly cancelling normal sums yield zero vectors. This is a visualization convention, not a topology test.

Normalization scales a vector by its maximum absolute component before taking its length. This supports finite vectors whose unscaled length would overflow, and tiny nonzero vectors. The zero vector and nonfinite vectors cannot be normalized. Primitive dot/cross/arithmetic operations are ordinary floating-point arithmetic; mesh-level operations check their required results for range problems.

## Bounding boxes

For all stored positions, including isolated positions:

```text
minimum = (min xi, min yi, min zi)
maximum = (max xi, max yi, max zi)
width  = maximum.x - minimum.x
height = maximum.y - minimum.y
depth  = maximum.z - minimum.z
```

Initialize both extrema from the first position, rather than from zero. Zero initialization gives wrong bounds for all-negative or all-positive geometry. An empty mesh has no box, represented by `std::optional<Bounds>`; it does not receive invented zero dimensions. Center uses `std::midpoint` to avoid overflow from `(minimum + maximum)/2`. An unrepresentable extent may be infinite in the library; the CLI labels that dimension unavailable.

## Convex polygon triangulation

A fan from a boundary vertex produces triangles `(v0, vi, vi+1)` for `i = 1..n-2`. The fan preserves winding and covers a planar convex polygon without interior overlap. Using it blindly on concave or crossing polygons can give wrong geometry.

Before triangulating a polygon with more than three vertices, the implementation:

1. Checks finite coordinates, index ranges, and unique indices.
2. Subtracts the first position and divides coordinates by the largest absolute local component. This makes checks relative to face size and reduces dependence on translation/scale.
3. Forms a unit normal from the first three corners; rejects nearly collinear initial corners.
4. Requires every corner's signed distance to the plane to be at most `1e-10` in normalized coordinates.
5. For each directed edge, requires every other corner to lie strictly on its inward side: `dot(cross(edge, corner - edge_start), normal) > 1e-12`.
6. Constructs and checks the fan triangles in the original coordinate system.

Checking all corners against every edge rejects concavity and self-crossing stars; checking only consecutive turn signs would not establish simplicity. Repeated positions and collinear corners fail the strict half-plane test. The checks are `O(n²)` for an `n`-corner polygon. Triangles use finite/nonzero/non-underflowed cross-product area without an additional aspect-ratio threshold.

Planarity within tolerance is an approximate floating-point policy, not a proof of exact coplanarity. Nearly degenerate convex polygons can be rejected. The parser never silently projects a visibly nonplanar face or invents a triangulation for a concave face.

## Topology and orientation

Each directed triangle edge `(i,j)` is stored under the canonical undirected key `(min(i,j), max(i,j))`, with incidence count and direction count:

- One incident triangle: boundary edge.
- Two incident triangles, opposite directions: locally consistent winding.
- Two incident triangles, same direction: winding inconsistency.
- More than two incident triangles: nonmanifold edge.

Edges alone miss pinched vertices. The link of a vertex is the graph of opposite edges from its incident triangles. A manifold interior vertex has one link cycle; a boundary vertex has one link path. Disconnected links, branching degrees, or invalid endpoint counts are reported as nonmanifold vertices. Two tetrahedra glued at one vertex therefore fail even though every edge has two incident faces.

Disjoint-set union computes connected components. Isolated vertices are counted separately; `surface_components` excludes them. Euler characteristic is `V - E + F`, including isolated vertices and triangulation diagonals. A tetrahedron or cube surface has characteristic 2, but that number alone proves neither manifoldness nor geometric validity.

## Enclosed volume

For a chosen origin `o`, a face contributes the oriented tetrahedron volume:

```text
Va = a - o, Vb = b - o, Vc = c - o
face contribution = dot(Va, cross(Vb, Vc)) / 6
signed volume = sum(face contributions)
enclosed volume = abs(signed volume), only after eligibility checks
```

The scalar triple product measures a parallelepiped; a tetrahedron occupies one sixth of it. For a closed surface, contributions cancel internal signed regions, producing the same answer for any origin. A nearby mesh vertex is used as origin to reduce cancellation after translation. The determinant uses `long double` intermediates and compensated summation; extended precision is platform-dependent.

Consistent outward winding gives positive signed volume. Reversing every face gives negative signed volume with unchanged enclosed magnitude. Local edge consistency cannot distinguish these orientations. Arbitrarily taking the absolute value of a sum for an open or intersecting surface would hide invalid assumptions; this implementation first checks eligibility.

Volume requires a nonempty single surface component, no boundary/nonmanifold edges, manifold vertex links, consistent winding, and no detected unexpected triangle intersection/contact. Isolated vertices are harmless. More than one surface component is unsupported: summing magnitudes can double-count overlaps, while summing signed components needs an explicit shell nesting/material convention. Disjoint, nested and overlapping multi-shell inputs all receive an unavailable result.

## Geometric intersections and tolerances

Before pair traversal, the default volume API checks that `F*(F-1)/2` fits its
1,000,000-pair work budget, dividing before multiplication to avoid overflow.
Exceeding the budget reports unavailable volume; trusted library callers can
provide another budget. This does not change the tetrahedron formula.

All triangle pairs whose exact axis-aligned boxes overlap are examined. Pair coordinates are translated and scaled locally. Separating-axis projections can establish disjointness; the axes include face normals, edge cross products, and in-plane edge normals for coplanar cases. Remaining pairs collect edge/plane contacts and projected coplanar segment crossings.

Contact is permitted only on a topologically shared vertex or edge. Any other contact is unsupported, including geometrically touching vertices that have distinct indices. Contacts exceeding a shared edge expose folded coplanar overlaps. The pair checks use distance/projection tolerance `1e-10`, a local area threshold `1e-12`, and a relative `1e-12` parallel-edge guard. A potential overlap with unresolved contacts is rejected conservatively.

A rigid-rotation regression caught division by a roundoff-sized determinant for collinear projected segments, inventing an intersection away from the true shared endpoint. The parallel-edge guard and near-plane guards prevent that failure. These predicates are finite-precision checks, not certified exact arithmetic. Features within tolerance may be treated as touching; sufficiently thin or poorly conditioned triangles can make volume unavailable. Applications needing guarantees on arbitrary adversarial floating-point inputs should use adaptive/exact predicates and broader geometric validation.

## Degeneracy and range

No face is silently skipped. Mesh validation rejects repeated indices, duplicate triangles of either winding, nonfinite positions, zero cross-product area, area that halves to zero by underflow, and overflow in required face-area intermediates. This includes geometric coincidences within a triangle even if indices differ. No welding or repair is performed.

Aggregated area overflow throws. Nonfinite, overflowing, zero or underflowed volume is unavailable. Input number parsing rejects nonfinite/out-of-range values rather than allowing NaN to bypass comparisons. Finite positions alone do not guarantee representable differences, products or dimensions. Tolerances are local and fixed; they do not compensate for coordinates that lost detail before parsing, such as `1e20 + 1` rounded to `1e20`.

## Analytical checks and questions

- Unit cube: six unit square faces give area 6; signed outward volume is 1.
- Unit-axis tetrahedron: three right-triangle faces each have area `1/2`, and the opposite face has area `sqrt(3)/2`; total `(3+sqrt(3))/2`, volume `1/6`.
- L prism: base area 3, perimeter 8, height 1; area `2*3 + 8*1 = 14`, volume 3. Its solid is nonconvex even though its input faces are supported triangles/quads.
- Square ring: a 4-by-4 footprint minus a 2-by-2 hole, height 1; volume `16-4=12`, area `2*(16-4)+16+8=48`. Its single connected boundary has Euler characteristic 0; eligibility is not restricted to genus-zero surfaces.
- Scaling all coordinates by positive `s` multiplies area by `s²` and volume by `s³`. Translation and rigid rotation preserve both.

Interview prompts: Why is volume origin-independent only for a closed surface? Why does consistent winding not prove outward orientation? Can Euler characteristic 2 certify an embedded sphere? Why does a self-crossing star defeat a turn-sign-only convexity test?

Exercises: derive the tetrahedron's opposite-face area; reverse one cube face and predict inconsistent edge counts; add an isolated vertex and predict changes in bounds, components and Euler characteristic; explain why absolute signed volume is insufficient for overlapping shells.
