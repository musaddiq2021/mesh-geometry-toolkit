#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mesh {

/// @brief Three Cartesian coordinates; units are chosen by the caller.
struct Vec3 {
    double x{}, y{}, z{};
    bool operator==(const Vec3&) const = default;
};

[[nodiscard]] constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
[[nodiscard]] constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
[[nodiscard]] constexpr Vec3 operator*(Vec3 v, double s) noexcept {
    return {v.x * s, v.y * s, v.z * s};
}
[[nodiscard]] constexpr Vec3 operator*(double s, Vec3 v) noexcept { return v * s; }
/// @throws std::invalid_argument If the divisor is zero or nonfinite.
[[nodiscard]] Vec3 operator/(Vec3 v, double s);
[[nodiscard]] constexpr double dot(Vec3 a, Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
[[nodiscard]] constexpr Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
/// @return Whether all three coordinates are finite.
[[nodiscard]] bool is_finite(Vec3 v) noexcept;
/// @brief Euclidean length using hypot; the result can exceed double's range.
[[nodiscard]] double length(Vec3 v) noexcept;
/// @brief Return a unit vector using scaling to avoid unnecessary overflow.
/// @throws std::invalid_argument If v is zero or nonfinite.
[[nodiscard]] Vec3 normalized(Vec3 v);

/// Counterclockwise vertex indices define the face normal by the right-hand rule.
using Triangle = std::array<std::size_t, 3>;

/// @brief Owned positions and zero-based triangles; mutation requires revalidation.
struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
};

enum class IssueCode {
    nonfinite_vertex,
    index_out_of_range,
    repeated_vertex,
    degenerate_triangle,
    duplicate_triangle,
    numeric_overflow
};

/// @brief Structural or numerical defect, with a zero-based element index.
struct ValidationIssue {
    IssueCode code;
    std::size_t element; // Vertex index for nonfinite_vertex; triangle index otherwise.
    std::string message;
};

/**
 * @brief Collect invalid indices, nonfinite coordinates, duplicates and unusable areas.
 * Empty meshes are valid. Duplicate faces ignore winding. Zero/underflowed area
 * is rejected without a geometric epsilon; area intermediates must be finite.
 * @return Issues referring to vertices or triangles, as specified by ValidationIssue.
 */
[[nodiscard]] std::vector<ValidationIssue> validate(const Mesh& mesh);
/// @throws std::invalid_argument If validate() finds any issue.
void require_valid(const Mesh& mesh);

/// @brief Axis-aligned extrema in coordinate units, including isolated vertices.
struct Bounds {
    Vec3 min;
    Vec3 max;
    /// Extents can be infinite if the span exceeds double's range.
    [[nodiscard]] Vec3 extent() const noexcept { return max - min; }
    [[nodiscard]] double width() const noexcept { return max.x - min.x; }
    [[nodiscard]] double height() const noexcept { return max.y - min.y; }
    [[nodiscard]] double depth() const noexcept { return max.z - min.z; }
    /// @brief Midpoint computed without overflowing min + max.
    [[nodiscard]] Vec3 center() const noexcept;
};

/// @return Bounds of all stored positions, or nullopt for an empty mesh.
/// @throws std::invalid_argument If structural/numerical validation fails.
[[nodiscard]] std::optional<Bounds> bounds(const Mesh& mesh);
/// @brief Sum half cross-product magnitudes, using compensated accumulation.
/// @return Surface area in squared coordinate units; zero for an empty mesh.
/// @throws std::invalid_argument For invalid geometry; std::overflow_error for sum overflow.
[[nodiscard]] double surface_area(const Mesh& mesh);
/// @return One unit normal per face in input order; invalid geometry throws.
[[nodiscard]] std::vector<Vec3> face_normals(const Mesh& mesh);
/// @brief Area-weighted unit normals; isolated vertices/cancelling sums yield zero.
/// @throws std::invalid_argument For invalid geometry; std::overflow_error for sum overflow.
[[nodiscard]] std::vector<Vec3> vertex_normals(const Mesh& mesh);

/// @brief Edge incidence, orientation, vertex-link and component statistics.
struct Topology {
    std::size_t edges{};
    std::size_t boundary_edges{};
    std::size_t nonmanifold_edges{};
    std::size_t nonmanifold_vertices{};
    std::size_t inconsistent_winding_edges{};
    std::size_t connected_components{}; // Includes isolated vertices.
    std::size_t isolated_vertices{};
    std::size_t surface_components{}; // Connected components excluding isolated vertices.
    std::int64_t euler_characteristic{}; // V - E + F, including isolated vertices.
    // Closed, consistently oriented edge incidence and manifold vertex links.
    // Does not test geometric intersections; use analyze_volume for eligibility.
    bool closed_consistently_oriented{};
};

/// @brief Inspect undirected edges and manifold vertex links (a path or cycle).
/// Does not test intersections; Euler characteristic alone does not establish validity.
/// @throws std::invalid_argument If structural validation fails.
[[nodiscard]] Topology topology(const Mesh& mesh);
/// @brief Volume eligibility with values only on success and a reason otherwise.
struct VolumeAnalysis {
    std::optional<double> signed_value;
    std::optional<double> enclosed_value;
    std::string reason; // Nonempty whenever unsupported; never a fabricated zero.
    [[nodiscard]] bool supported() const noexcept { return signed_value.has_value(); }
};

/**
 * @brief Check eligibility, then sum signed tetrahedron volumes in cubed units.
 * Supports one closed, consistently oriented manifold surface. Multiple shells
 * and unexpected contacts are unsupported. Predicates use local floating-point
 * tolerances; they are not exact-arithmetic certificates.
 * @param max_triangle_pairs Upper bound on the quadratic intersection pass.
 * @return Signed volume and its absolute value, or neither value with a reason.
 * @throws std::invalid_argument If structural validation fails.
 */
[[nodiscard]] VolumeAnalysis analyze_volume(const Mesh& mesh, std::size_t max_triangle_pairs = 1'000'000);
/// @brief Default-budget wrapper; outward winding gives positive signed volume.
/// @throws std::invalid_argument If geometry is invalid or volume unsupported.
[[nodiscard]] double signed_volume(const Mesh& mesh);

} // namespace mesh
