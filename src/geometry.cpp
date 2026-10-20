#include "mesh/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <utility>

namespace mesh {
namespace {

Vec3 area_vector(const Mesh& m, const Triangle& t) {
    return cross(m.vertices[t[1]] - m.vertices[t[0]], m.vertices[t[2]] - m.vertices[t[0]]);
}

class Components {
public:
    explicit Components(std::size_t n) : parent_(n), rank_(n) {
        std::iota(parent_.begin(), parent_.end(), std::size_t{0});
    }
    std::size_t root(std::size_t v) {
        while (parent_[v] != v) {
            parent_[v] = parent_[parent_[v]];
            v = parent_[v];
        }
        return v;
    }
    void join(std::size_t a, std::size_t b) {
        a = root(a);
        b = root(b);
        if (a == b) return;
        if (rank_[a] < rank_[b]) std::swap(a, b);
        parent_[b] = a;
        if (rank_[a] == rank_[b]) ++rank_[a];
    }
private:
    std::vector<std::size_t> parent_;
    std::vector<unsigned> rank_;
};

// Neumaier summation reduces cancellation across triangles of different sizes.
class Sum {
public:
    void add(double value) {
        const double next = sum_ + value;
        correction_ += std::abs(sum_) >= std::abs(value)
            ? (sum_ - next) + value : (value - next) + sum_;
        sum_ = next;
    }
    [[nodiscard]] double value() const { return sum_ + correction_; }
private:
    double sum_{};
    double correction_{};
};

void check_finite(double value, const char* operation) {
    if (!std::isfinite(value)) throw std::overflow_error(operation);
}

} // namespace

Vec3 operator/(Vec3 v, double s) {
    if (s == 0.0 || !std::isfinite(s)) {
        throw std::invalid_argument("Vector divisor must be finite and nonzero");
    }
    return {v.x / s, v.y / s, v.z / s};
}

bool is_finite(Vec3 v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

double length(Vec3 v) noexcept { return std::hypot(v.x, v.y, v.z); }

Vec3 normalized(Vec3 v) {
    if (!is_finite(v)) throw std::invalid_argument("Cannot normalize a nonfinite vector");
    const double scale = std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)});
    if (scale == 0.0) throw std::invalid_argument("Cannot normalize a zero vector");
    const Vec3 scaled = v / scale;
    return scaled / length(scaled);
}

std::vector<ValidationIssue> validate(const Mesh& m) {
    std::vector<ValidationIssue> issues;
    for (std::size_t i = 0; i < m.vertices.size(); ++i) {
        if (!is_finite(m.vertices[i])) {
            issues.push_back({IssueCode::nonfinite_vertex, i, "Vertex " + std::to_string(i) + " is nonfinite"});
        }
    }
    std::set<Triangle> seen;
    for (std::size_t i = 0; i < m.triangles.size(); ++i) {
        const auto& t = m.triangles[i];
        const std::string prefix = "Triangle " + std::to_string(i);
        if (std::any_of(t.begin(), t.end(), [&](auto v) { return v >= m.vertices.size(); })) {
            issues.push_back({IssueCode::index_out_of_range, i, prefix + " has an out-of-range vertex index"});
            continue;
        }
        if (t[0] == t[1] || t[1] == t[2] || t[2] == t[0]) {
            issues.push_back({IssueCode::repeated_vertex, i, prefix + " repeats a vertex index"});
            continue;
        }
        auto key = t;
        std::sort(key.begin(), key.end());
        if (!seen.insert(key).second) {
            issues.push_back({IssueCode::duplicate_triangle, i, prefix + " duplicates an earlier triangle"});
        }
        if (!is_finite(m.vertices[t[0]]) || !is_finite(m.vertices[t[1]]) || !is_finite(m.vertices[t[2]])) continue;
        const Vec3 av = area_vector(m, t);
        const double magnitude = length(av);
        if (!is_finite(av) || !std::isfinite(magnitude)) {
            issues.push_back({IssueCode::numeric_overflow, i, prefix + " exceeds the numeric range for area computation"});
        } else if (magnitude == 0.0 || magnitude * 0.5 == 0.0) {
            issues.push_back({IssueCode::degenerate_triangle, i, prefix + " has zero area or area underflow at double precision"});
        }
    }
    return issues;
}

void require_valid(const Mesh& m) {
    const auto issues = validate(m);
    if (!issues.empty()) throw std::invalid_argument(issues.front().message);
}

Vec3 Bounds::center() const noexcept {
    return {std::midpoint(min.x, max.x), std::midpoint(min.y, max.y), std::midpoint(min.z, max.z)};
}

std::optional<Bounds> bounds(const Mesh& m) {
    require_valid(m);
    if (m.vertices.empty()) return std::nullopt;
    Bounds result{m.vertices.front(), m.vertices.front()};
    for (const auto v : m.vertices) {
        result.min = {std::min(result.min.x, v.x), std::min(result.min.y, v.y), std::min(result.min.z, v.z)};
        result.max = {std::max(result.max.x, v.x), std::max(result.max.y, v.y), std::max(result.max.z, v.z)};
    }
    return result;
}

double surface_area(const Mesh& m) {
    require_valid(m);
    Sum area;
    for (const auto& t : m.triangles) area.add(length(area_vector(m, t)) * 0.5);
    check_finite(area.value(), "Surface area exceeds double precision range");
    return area.value();
}

std::vector<Vec3> face_normals(const Mesh& m) {
    require_valid(m);
    std::vector<Vec3> result;
    result.reserve(m.triangles.size());
    for (const auto& t : m.triangles) result.push_back(normalized(area_vector(m, t)));
    return result;
}

std::vector<Vec3> vertex_normals(const Mesh& m) {
    require_valid(m);
    std::vector<Vec3> result(m.vertices.size());
    for (const auto& t : m.triangles) {
        const Vec3 av = area_vector(m, t);
        for (const auto v : t) {
            result[v] = result[v] + av;
            if (!is_finite(result[v])) throw std::overflow_error("Vertex normal sum exceeds double precision range");
        }
    }
    for (auto& v : result) {
        if (v != Vec3{}) v = normalized(v);
    }
    return result;
}

Topology topology(const Mesh& m) {
    require_valid(m);
    struct Incidence { std::size_t count{}, forward{}; };
    std::map<std::pair<std::size_t, std::size_t>, Incidence> edges;
    Components components(m.vertices.size());
    std::vector<bool> used(m.vertices.size());
    std::vector<std::vector<std::pair<std::size_t, std::size_t>>> links(m.vertices.size());
    for (const auto& t : m.triangles) {
        for (std::size_t i = 0; i < 3; ++i) {
            const auto a = t[i];
            const auto b = t[(i + 1) % 3];
            auto& edge = edges[{std::min(a, b), std::max(a, b)}];
            ++edge.count;
            edge.forward += a < b ? 1U : 0U;
            used[a] = true;
            links[a].push_back({b, t[(i + 2) % 3]});
            components.join(a, b);
        }
    }
    Topology result;
    result.edges = edges.size();
    for (const auto& [key, edge] : edges) {
        (void)key;
        if (edge.count == 1) ++result.boundary_edges;
        if (edge.count > 2) ++result.nonmanifold_edges;
        if (edge.count == 2 && edge.forward != 1) ++result.inconsistent_winding_edges;
    }
    for (std::size_t v = 0; v < m.vertices.size(); ++v) {
        if (components.root(v) == v) ++result.connected_components;
        if (!used[v]) ++result.isolated_vertices;
        if (!used[v]) continue;
        // A manifold vertex link is one cycle (interior) or one path (boundary).
        std::map<std::size_t, std::vector<std::size_t>> adjacency;
        for (const auto& [a, b] : links[v]) {
            adjacency[a].push_back(b);
            adjacency[b].push_back(a);
        }
        std::size_t endpoints{};
        bool manifold = true;
        for (const auto& [neighbor, adjacent] : adjacency) {
            (void)neighbor;
            if (adjacent.size() == 1) ++endpoints;
            else if (adjacent.size() != 2) manifold = false;
        }
        std::set<std::size_t> reached;
        std::vector<std::size_t> pending{adjacency.begin()->first};
        while (!pending.empty()) {
            const auto neighbor = pending.back();
            pending.pop_back();
            if (!reached.insert(neighbor).second) continue;
            for (const auto adjacent : adjacency.at(neighbor)) pending.push_back(adjacent);
        }
        if (!manifold || (endpoints != 0 && endpoints != 2) || reached.size() != adjacency.size()) {
            ++result.nonmanifold_vertices;
        }
    }
    result.surface_components = result.connected_components - result.isolated_vertices;
    constexpr auto limit = static_cast<std::uintmax_t>(std::numeric_limits<std::int64_t>::max());
    if (m.vertices.size() > limit || m.triangles.size() > limit || result.edges > limit) {
        throw std::overflow_error("Mesh is too large for Euler characteristic");
    }
    // V - E is in [-INT64_MAX, INT64_MAX]; guard addition of F separately.
    const auto ve = static_cast<std::int64_t>(m.vertices.size()) - static_cast<std::int64_t>(result.edges);
    const auto f = static_cast<std::int64_t>(m.triangles.size());
    if (ve > std::numeric_limits<std::int64_t>::max() - f) {
        throw std::overflow_error("Euler characteristic exceeds int64 range");
    }
    result.euler_characteristic = ve + f;
    result.closed_consistently_oriented = !m.triangles.empty() && result.boundary_edges == 0
        && result.nonmanifold_edges == 0 && result.nonmanifold_vertices == 0 && result.inconsistent_winding_edges == 0;
    return result;
}

double signed_volume(const Mesh& m) {
    const auto result = analyze_volume(m);
    if (!result.supported()) throw std::invalid_argument(result.reason);
    return *result.signed_value;
}

} // namespace mesh
