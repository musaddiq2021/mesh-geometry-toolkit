#include "mesh/triangulation.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace mesh {

std::vector<Triangle> triangulate_convex_polygon(
    std::span<const Vec3> vertices, std::span<const std::size_t> polygon) {
    if (polygon.size() < 3) throw std::invalid_argument("A face needs at least three vertices");
    std::set<std::size_t> unique;
    for (const auto index : polygon) {
        if (index >= vertices.size()) throw std::invalid_argument("Polygon vertex index is out of range");
        if (!is_finite(vertices[index])) throw std::invalid_argument("Polygon coordinates must be finite");
        if (!unique.insert(index).second) throw std::invalid_argument("Polygon repeats a vertex index");
    }
    if (polygon.size() > 3) {
        std::vector<Vec3> local;
        local.reserve(polygon.size());
        double scale{};
        for (const auto index : polygon) {
            const auto p = vertices[index] - vertices[polygon[0]];
            if (!is_finite(p)) throw std::invalid_argument("Polygon coordinate differences overflow");
            scale = std::max({scale, std::abs(p.x), std::abs(p.y), std::abs(p.z)});
            local.push_back(p);
        }
        if (scale == 0) throw std::invalid_argument("Polygon has coincident vertices");
        for (auto& p : local) p = p / scale;
        const auto av = cross(local[1], local[2]);
        if (length(av) <= 1e-12) throw std::invalid_argument("Polygon has collinear or numerically ambiguous corners");
        const auto normal = normalized(av);
        for (const auto p : local) {
            if (std::abs(dot(p, normal)) > 1e-10) throw std::invalid_argument("Polygon is not planar");
        }
        // All other vertices must be strictly on the inward side of EVERY edge.
        // Unlike a turn-sign-only test, this also rejects self-crossing stars.
        for (std::size_t i = 0; i < local.size(); ++i) {
            const auto next = (i + 1) % local.size();
            const auto edge = local[next] - local[i];
            for (std::size_t j = 0; j < local.size(); ++j) {
                if (j == i || j == next) continue;
                if (dot(cross(edge, local[j] - local[i]), normal) <= 1e-12) {
                    throw std::invalid_argument("Polygon must be simple and strictly convex; concave, crossing, repeated or collinear corners are unsupported");
                }
            }
        }
    }
    std::vector<Triangle> result;
    result.reserve(polygon.size() - 2);
    for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
        const Triangle triangle{polygon[0], polygon[i], polygon[i + 1]};
        const auto av = cross(vertices[triangle[1]] - vertices[triangle[0]],
                              vertices[triangle[2]] - vertices[triangle[0]]);
        if (!is_finite(av) || !std::isfinite(length(av))) {
            throw std::invalid_argument("Triangle area exceeds the double precision range");
        }
        if (length(av) * 0.5 == 0) throw std::invalid_argument("Face contains a zero-area or area-underflow triangle at double precision");
        result.push_back(triangle);
    }
    return result;
}

} // namespace mesh
