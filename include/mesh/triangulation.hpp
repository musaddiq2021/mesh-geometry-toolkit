#pragma once

#include "mesh/geometry.hpp"

#include <span>
#include <vector>

namespace mesh {

/**
 * @brief Validate a boundary-ordered face, then emit a winding-preserving fan.
 * All other corners must lie inside every directed edge half-plane. This rejects
 * concavity and crossing stars; consecutive turn signs alone are insufficient.
 * @param vertices Position array referenced by polygon's zero-based indices.
 * @param polygon Triangle or strictly convex planar polygon, without repeated corners.
 * @return n-2 triangles in fan order.
 * @throws std::invalid_argument For bad indices or unsupported/nonfinite/degenerate geometry.
 * @note Checks are O(n squared), with normalized plane tolerance 1e-10 and
 * strict half-plane margin 1e-12. Collinear/ambiguous corners are unsupported.
 */
[[nodiscard]] std::vector<Triangle> triangulate_convex_polygon(
    std::span<const Vec3> vertices, std::span<const std::size_t> polygon);

} // namespace mesh
