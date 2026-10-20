#include "mesh/geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace mesh {
namespace {

constexpr double tolerance = 1e-10;
using Points = std::array<Vec3, 3>;

bool boxes_overlap(const Points& a, const Points& b) {
    for (int axis = 0; axis < 3; ++axis) {
        const auto coordinate = [axis](Vec3 p) { return axis == 0 ? p.x : (axis == 1 ? p.y : p.z); };
        double amin = coordinate(a[0]), amax = amin;
        double bmin = coordinate(b[0]), bmax = bmin;
        for (std::size_t i = 1; i < 3; ++i) {
            amin = std::min(amin, coordinate(a[i]));
            amax = std::max(amax, coordinate(a[i]));
            bmin = std::min(bmin, coordinate(b[i]));
            bmax = std::max(bmax, coordinate(b[i]));
        }
        if (amax < bmin || bmax < amin) return false;
    }
    return true;
}

bool separates(Vec3 axis, const Points& a, const Points& b) {
    if (axis == Vec3{}) return false;
    axis = normalized(axis);
    double amin = dot(axis, a[0]), amax = amin;
    double bmin = dot(axis, b[0]), bmax = bmin;
    for (std::size_t i = 1; i < 3; ++i) {
        amin = std::min(amin, dot(axis, a[i]));
        amax = std::max(amax, dot(axis, a[i]));
        bmin = std::min(bmin, dot(axis, b[i]));
        bmax = std::max(bmax, dot(axis, b[i]));
    }
    return amax < bmin - tolerance || bmax < amin - tolerance;
}

bool inside(Vec3 p, const Points& triangle, Vec3 normal) {
    for (std::size_t i = 0; i < 3; ++i) {
        if (dot(cross(triangle[(i + 1) % 3] - triangle[i], p - triangle[i]), normal) < -tolerance) return false;
    }
    return true;
}

void plane_contacts(const Points& a, const Points& b, Vec3 normal, std::vector<Vec3>& contacts) {
    for (std::size_t i = 0; i < 3; ++i) {
        const Vec3 p = a[i], q = a[(i + 1) % 3];
        const double dp = dot(p - b[0], normal), dq = dot(q - b[0], normal);
        if (std::abs(dp) <= tolerance && inside(p, b, normal)) contacts.push_back(p);
        if ((dp > tolerance && dq < -tolerance) || (dp < -tolerance && dq > tolerance)) {
            const Vec3 contact = p + (q - p) * (dp / (dp - dq));
            if (inside(contact, b, normal)) contacts.push_back(contact);
        }
    }
}

struct Point2 { double x, y; };
Point2 operator-(Point2 a, Point2 b) { return {a.x - b.x, a.y - b.y}; }
long double cross2(Point2 a, Point2 b) {
    return static_cast<long double>(a.x) * b.y - static_cast<long double>(a.y) * b.x;
}

// Coplanar edge crossings create intersection vertices not captured by vertex
// containment (e.g. two overlapping triangles with all vertices outside).
void coplanar_contacts(const Points& a, const Points& b, Vec3 normal, Vec3 bnormal, std::vector<Vec3>& contacts) {
    const int drop = std::abs(normal.x) >= std::max(std::abs(normal.y), std::abs(normal.z)) ? 0
        : (std::abs(normal.y) >= std::abs(normal.z) ? 1 : 2);
    const auto project = [drop](Vec3 p) -> Point2 {
        if (drop == 0) return {p.y, p.z};
        if (drop == 1) return {p.x, p.z};
        return {p.x, p.y};
    };
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            const Vec3 p = a[i], q = a[(i + 1) % 3];
            const Vec3 r = b[j], s = b[(j + 1) % 3];
            const auto u = project(q) - project(p), v = project(s) - project(r);
            const auto offset = project(r) - project(p);
            const auto denominator = cross2(u, v);
            const long double parallel_margin = 1e-12L * std::hypot(u.x, u.y) * std::hypot(v.x, v.y);
            // Do not divide by a roundoff-sized denominator for rotated
            // collinear edges: it invents intersections far from a shared end.
            if (std::abs(denominator) > parallel_margin) {
                const auto t = cross2(offset, v) / denominator;
                const auto w = cross2(offset, u) / denominator;
                if (t >= -tolerance && t <= 1 + tolerance && w >= -tolerance && w <= 1 + tolerance) {
                    contacts.push_back(p + (q - p) * static_cast<double>(std::clamp(t, 0.0L, 1.0L)));
                }
            } else if (std::abs(cross2(offset, u)) <= tolerance) {
                // Collinear overlap endpoints are triangle vertices, collected
                // by plane_contacts. No independent crossing is needed.
                for (const auto point : {p, q}) if (inside(point, b, bnormal)) contacts.push_back(point);
                for (const auto point : {r, s}) if (inside(point, a, normal)) contacts.push_back(point);
            }
        }
    }
}

bool permitted_contact(Vec3 p, const std::vector<Vec3>& shared) {
    if (shared.empty()) return false;
    if (shared.size() == 1) return length(p - shared[0]) <= tolerance;
    const auto edge = shared[1] - shared[0];
    const auto squared = dot(edge, edge);
    if (squared == 0) return false;
    const double t = dot(p - shared[0], edge) / squared;
    return t >= -tolerance && t <= 1 + tolerance && length(p - (shared[0] + edge * t)) <= tolerance;
}

// True means an unexpected intersection/contact OR an unresolved numeric case.
bool unexpected_contact(const Mesh& m, const Triangle& ta, const Triangle& tb) {
    Points a{m.vertices[ta[0]], m.vertices[ta[1]], m.vertices[ta[2]]};
    Points b{m.vertices[tb[0]], m.vertices[tb[1]], m.vertices[tb[2]]};
    if (!boxes_overlap(a, b)) return false;
    const auto origin = a[0];
    double scale{};
    for (auto* points : {&a, &b}) {
        for (auto& p : *points) {
            p = p - origin;
            if (!is_finite(p)) return true;
            scale = std::max({scale, std::abs(p.x), std::abs(p.y), std::abs(p.z)});
        }
    }
    if (scale == 0) return true;
    for (auto* points : {&a, &b}) for (auto& p : *points) p = p / scale;
    const auto av = cross(a[1] - a[0], a[2] - a[0]);
    const auto bv = cross(b[1] - b[0], b[2] - b[0]);
    if (length(av) <= 1e-12 || length(bv) <= 1e-12) return true;
    const Vec3 na = normalized(av), nb = normalized(bv);
    if (separates(na, a, b) || separates(nb, a, b)) return false;
    for (std::size_t i = 0; i < 3; ++i) {
        const Vec3 ea = a[(i + 1) % 3] - a[i];
        const Vec3 eb = b[(i + 1) % 3] - b[i];
        if (separates(cross(na, ea), a, b) || separates(cross(nb, eb), a, b)) return false;
        for (std::size_t j = 0; j < 3; ++j) {
            if (separates(cross(ea, b[(j + 1) % 3] - b[j]), a, b)) return false;
        }
    }
    std::vector<Vec3> shared;
    for (std::size_t i = 0; i < 3; ++i) {
        for (const auto index : tb) if (ta[i] == index) shared.push_back(a[i]);
    }
    std::vector<Vec3> contacts;
    plane_contacts(a, b, nb, contacts);
    plane_contacts(b, a, na, contacts);
    const bool coplanar = std::all_of(b.begin(), b.end(), [&](Vec3 p) { return std::abs(dot(p - a[0], na)) <= tolerance; })
        && std::all_of(a.begin(), a.end(), [&](Vec3 p) { return std::abs(dot(p - b[0], nb)) <= tolerance; });
    if (coplanar) coplanar_contacts(a, b, na, nb, contacts);
    if (contacts.empty()) return true; // SAT overlap without resolved contacts: reject uncertainty.
    return std::any_of(contacts.begin(), contacts.end(), [&](Vec3 p) { return !permitted_contact(p, shared); });
}

VolumeAnalysis unavailable(const std::string& reason) { return {std::nullopt, std::nullopt, reason}; }

} // namespace

VolumeAnalysis analyze_volume(const Mesh& m, std::size_t max_triangle_pairs) {
    const auto t = topology(m); // Validates indices, finiteness and degeneracy first.
    if (m.triangles.empty()) return unavailable("Mesh has no surface triangles");
    if (t.nonmanifold_edges) return unavailable("Mesh has non-manifold edges");
    if (t.nonmanifold_vertices) return unavailable("Mesh has non-manifold vertex links");
    if (t.boundary_edges) return unavailable("Mesh is open: boundary edges are present");
    if (t.inconsistent_winding_edges) return unavailable("Mesh face winding is inconsistent");
    if (t.surface_components != 1) return unavailable("Multiple surface components are unsupported (overlap/nesting is ambiguous)");
    // Divide before multiplying so a hostile count cannot overflow n*(n-1)/2.
    const auto count = m.triangles.size();
    const auto left = count % 2 == 0 ? count / 2 : count;
    const auto right = count % 2 == 0 ? count - 1 : (count - 1) / 2;
    if (right != 0 && left > max_triangle_pairs / right) {
        return unavailable("Intersection check budget exceeded");
    }
    for (std::size_t a = 0; a < m.triangles.size(); ++a) {
        for (std::size_t b = a + 1; b < m.triangles.size(); ++b) {
            if (unexpected_contact(m, m.triangles[a], m.triangles[b])) {
                return unavailable("Self-intersection, unexpected surface contact or numerically ambiguous triangles "
                    + std::to_string(a) + " and " + std::to_string(b));
            }
        }
    }
    const Vec3 origin = m.vertices[m.triangles.front()[0]];
    // Long double intermediates improve summation and range where the platform
    // provides extended precision. Public results must remain finite doubles.
    long double sum{}, correction{};
    for (const auto& triangle : m.triangles) {
        const Vec3 a = m.vertices[triangle[0]] - origin;
        const Vec3 b = m.vertices[triangle[1]] - origin;
        const Vec3 c = m.vertices[triangle[2]] - origin;
        if (!is_finite(a) || !is_finite(b) || !is_finite(c)) return unavailable("Volume coordinate differences overflow");
        const long double contribution = (
            static_cast<long double>(a.x) * (static_cast<long double>(b.y) * c.z - static_cast<long double>(b.z) * c.y)
          + static_cast<long double>(a.y) * (static_cast<long double>(b.z) * c.x - static_cast<long double>(b.x) * c.z)
          + static_cast<long double>(a.z) * (static_cast<long double>(b.x) * c.y - static_cast<long double>(b.y) * c.x)) / 6;
        const auto next = sum + contribution;
        correction += std::abs(sum) >= std::abs(contribution)
            ? (sum - next) + contribution : (contribution - next) + sum;
        sum = next;
    }
    const auto total = sum + correction;
    if (!std::isfinite(total) || std::abs(total) > std::numeric_limits<double>::max()) {
        return unavailable("Volume exceeds the double precision range");
    }
    const auto value = static_cast<double>(total);
    if (value == 0) return unavailable("Volume is zero or underflows double precision");
    return {value, std::abs(value), {}};
}

} // namespace mesh
