#include "mesh/geometry.hpp"
#include "mesh/obj.hpp"
#include "mesh/triangulation.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <random>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <utility>
#include <vector>

namespace {

struct TestFailure : std::runtime_error { using std::runtime_error::runtime_error; };

#define CHECK(expression) do { if (!(expression)) throw TestFailure( \
    std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " + #expression); } while (false)

void near(double actual, double expected, double tolerance = 1e-12) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance * std::max(1.0, std::abs(expected))) {
        throw TestFailure("Expected " + std::to_string(expected) + ", got " + std::to_string(actual));
    }
}

void near(mesh::Vec3 actual, mesh::Vec3 expected, double tolerance = 1e-12) {
    near(actual.x, expected.x, tolerance);
    near(actual.y, expected.y, tolerance);
    near(actual.z, expected.z, tolerance);
}

void relative_near(double actual, double expected, long double tolerance = 1e-12L) {
    CHECK(std::isfinite(actual) && std::isfinite(expected) && expected != 0);
    CHECK(std::abs((static_cast<long double>(actual) - expected) / expected) <= tolerance);
}

template <typename Exception = std::exception, typename Function>
void throws(Function&& function) {
    bool caught = false;
    try { function(); }
    catch (const Exception&) { caught = true; }
    CHECK(caught);
}

mesh::Mesh triangle() {
    return {{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}, {{0, 1, 2}}};
}

mesh::Mesh tetrahedron() {
    return {{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
            {{0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3}}};
}

mesh::Mesh cube() {
    return {{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
             {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
            {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7},
             {0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5},
             {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}}};
}

mesh::Mesh l_prism() {
    mesh::Mesh result;
    const std::vector<mesh::Vec3> boundary{{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {1, 1, 0}, {1, 2, 0}, {0, 2, 0}};
    result.vertices = boundary;
    for (const auto p : boundary) result.vertices.push_back(p + mesh::Vec3{0, 0, 1});
    const std::vector<mesh::Triangle> cap{{0, 1, 3}, {1, 2, 3}, {0, 3, 5}, {3, 4, 5}};
    for (const auto& t : cap) {
        result.triangles.push_back({t[0], t[2], t[1]});
        result.triangles.push_back({t[0] + 6, t[1] + 6, t[2] + 6});
    }
    for (std::size_t i = 0; i < boundary.size(); ++i) {
        const auto j = (i + 1) % boundary.size();
        result.triangles.push_back({i, j, j + 6});
        result.triangles.push_back({i, j + 6, i + 6});
    }
    return result;
}

mesh::Mesh square_ring() {
    mesh::Mesh result;
    result.vertices = {{-2,-2,0},{2,-2,0},{2,2,0},{-2,2,0},
                       {-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};
    for (std::size_t i = 0; i < 8; ++i) result.vertices.push_back(result.vertices[i] + mesh::Vec3{0,0,1});
    const auto quad = [&](std::size_t a, std::size_t b, std::size_t c, std::size_t d) {
        result.triangles.push_back({a,b,c});
        result.triangles.push_back({a,c,d});
    };
    for (std::size_t i = 0; i < 4; ++i) {
        const auto j = (i + 1) % 4;
        quad(i+8,j+8,j+12,i+12); // Top annulus, normal +Z.
        quad(i,i+4,j+4,j);       // Bottom annulus, normal -Z.
        quad(i,j,j+8,i+8);       // Outer wall.
        quad(j+4,i+4,i+12,j+12); // Inner wall, normal into the hole.
    }
    return result;
}

mesh::Mesh parse(const std::string& text) {
    std::istringstream input(text);
    return mesh::read_obj(input);
}

bool has_issue(const mesh::Mesh& m, mesh::IssueCode code) {
    const auto issues = mesh::validate(m);
    return std::any_of(issues.begin(), issues.end(), [=](const auto& issue) { return issue.code == code; });
}

const std::string positions = "v 0 0 0\nv 1 0 0\nv 0 1 0\n";

class BrokenInput : public std::streambuf {
    int_type underflow() override { throw std::runtime_error("Input failure"); }
};

class BrokenOutput : public std::streambuf {
    int_type overflow(int_type) override { return traits_type::eof(); }
};

class InterruptedInput : public std::streambuf {
public:
    InterruptedInput() { setg(prefix_.data(), prefix_.data(), prefix_.data() + prefix_.size()); }
private:
    std::string prefix_{"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"};
    int_type underflow() override { throw std::runtime_error("Read interrupted after valid data"); }
};

class CommaDecimal : public std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
};

} // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"vector algebra", [] {
            near(mesh::Vec3{1, 2, 3} + mesh::Vec3{3, 2, 1}, {4, 4, 4});
            near(mesh::Vec3{1, 2, 3} - mesh::Vec3{3, 2, 1}, {-2, 0, 2});
            near(2.0 * mesh::Vec3{1, 2, 3}, {2, 4, 6});
            near(mesh::Vec3{2, 4, 6} / 2, {1, 2, 3});
            near(mesh::dot({1, 2, 3}, {4, 5, 6}), 32);
            near(mesh::cross({1, 0, 0}, {0, 1, 0}), {0, 0, 1});
            near(mesh::length({3, 4, 0}), 5);
            near(mesh::normalized({3, 4, 0}), {0.6, 0.8, 0});
        }},
        {"normalization extremes and invalid input", [] {
            const double large = std::numeric_limits<double>::max();
            near(mesh::length(mesh::normalized({large, large, large})), 1);
            near(mesh::normalized({1e-310, 0, 0}), {1, 0, 0});
            throws<std::invalid_argument>([] { (void)mesh::normalized({}); });
            throws<std::invalid_argument>([] { (void)(mesh::Vec3{} / 0); });
            throws<std::invalid_argument>([] { (void)(mesh::Vec3{} / std::numeric_limits<double>::infinity()); });
            throws<std::invalid_argument>([] { (void)mesh::normalized({std::numeric_limits<double>::quiet_NaN(), 0, 0}); });
        }},
        {"empty mesh contracts", [] {
            const mesh::Mesh empty;
            CHECK(mesh::validate(empty).empty());
            CHECK(!mesh::bounds(empty));
            near(mesh::surface_area(empty), 0);
            CHECK(mesh::face_normals(empty).empty());
            CHECK(mesh::vertex_normals(empty).empty());
            const auto t = mesh::topology(empty);
            CHECK(t.edges == 0 && t.connected_components == 0 && t.euler_characteristic == 0);
            CHECK(!t.closed_consistently_oriented);
            throws<std::invalid_argument>([&] { (void)mesh::signed_volume(empty); });
        }},
        {"triangle geometry and normals", [] {
            const auto m = triangle();
            CHECK(mesh::validate(m).empty());
            near(mesh::surface_area(m), 0.5);
            const auto box = mesh::bounds(m);
            CHECK(box.has_value());
            near(box->min, {0, 0, 0});
            near(box->max, {1, 1, 0});
            near(box->center(), {0.5, 0.5, 0});
            near(box->extent(), {1, 1, 0});
            const auto faces = mesh::face_normals(m), vertices = mesh::vertex_normals(m);
            CHECK(faces.size() == m.triangles.size() && vertices.size() == m.vertices.size());
            near(faces.at(0), {0, 0, 1});
            for (const auto n : vertices) near(n, {0, 0, 1});
        }},
        {"bounds include isolated vertices and use safe midpoint", [] {
            mesh::Mesh m{{{-1e308, 0, 0}, {1e308, 2, 4}}, {}};
            const auto box = mesh::bounds(m);
            near(box->center(), {0, 1, 2});
            CHECK(mesh::topology(m).isolated_vertices == 2);
            CHECK(mesh::topology(m).connected_components == 2);
            for (const auto n : mesh::vertex_normals(m)) near(n, {});
        }},
        {"out-of-range indices rejected by all consumers", [] {
            auto m = triangle();
            m.triangles[0][2] = std::numeric_limits<std::size_t>::max();
            CHECK(has_issue(m, mesh::IssueCode::index_out_of_range));
            throws<std::invalid_argument>([&] { mesh::require_valid(m); });
            throws<std::invalid_argument>([&] { (void)mesh::bounds(m); });
            throws<std::invalid_argument>([&] { (void)mesh::surface_area(m); });
            throws<std::invalid_argument>([&] { (void)mesh::face_normals(m); });
            throws<std::invalid_argument>([&] { (void)mesh::vertex_normals(m); });
            throws<std::invalid_argument>([&] { (void)mesh::topology(m); });
            throws<std::invalid_argument>([&] { (void)mesh::signed_volume(m); });
        }},
        {"repeated indices and collinear geometry", [] {
            auto m = triangle();
            m.triangles[0] = {0, 0, 1};
            CHECK(has_issue(m, mesh::IssueCode::repeated_vertex));
            m.triangles[0] = {0, 1, 2};
            m.vertices[2] = {2, 0, 0};
            CHECK(has_issue(m, mesh::IssueCode::degenerate_triangle));
            m.vertices[2] = m.vertices[1];
            CHECK(has_issue(m, mesh::IssueCode::degenerate_triangle));
        }},
        {"nonfinite and numeric overflow diagnostics", [] {
            auto m = triangle();
            m.vertices.push_back({std::numeric_limits<double>::infinity(), 0, 0});
            CHECK(has_issue(m, mesh::IssueCode::nonfinite_vertex));
            m = triangle();
            m.vertices[1] = {1e300, 0, 0};
            m.vertices[2] = {0, 1e300, 0};
            CHECK(has_issue(m, mesh::IssueCode::numeric_overflow));
            throws<std::invalid_argument>([&] { (void)mesh::surface_area(m); });
        }},
        {"duplicate triangles ignore permutation and winding", [] {
            auto m = triangle();
            m.triangles.push_back({2, 1, 0});
            CHECK(has_issue(m, mesh::IssueCode::duplicate_triangle));
            m.triangles[1] = {1, 2, 0};
            CHECK(has_issue(m, mesh::IssueCode::duplicate_triangle));
        }},
        {"triangle topology and open volume rejection", [] {
            const auto m = triangle();
            const auto t = mesh::topology(m);
            CHECK(t.edges == 3 && t.boundary_edges == 3);
            CHECK(t.nonmanifold_edges == 0 && t.inconsistent_winding_edges == 0);
            CHECK(t.connected_components == 1 && t.euler_characteristic == 1);
            CHECK(!t.closed_consistently_oriented);
            throws<std::invalid_argument>([&] { (void)mesh::signed_volume(m); });
        }},
        {"square shared edge and winding inconsistency", [] {
            mesh::Mesh m{{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}, {{0, 1, 2}, {0, 2, 3}}};
            near(mesh::surface_area(m), 1);
            auto t = mesh::topology(m);
            CHECK(t.edges == 5 && t.boundary_edges == 4 && t.inconsistent_winding_edges == 0);
            std::swap(m.triangles[1][0], m.triangles[1][1]);
            t = mesh::topology(m);
            CHECK(t.inconsistent_winding_edges == 1);
        }},
        {"nonmanifold edge detection", [] {
            mesh::Mesh m{{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, -1, 0}},
                         {{0, 1, 2}, {1, 0, 3}, {0, 1, 4}}};
            const auto t = mesh::topology(m);
            CHECK(t.nonmanifold_edges == 1 && t.boundary_edges == 6);
            CHECK(!t.closed_consistently_oriented);
            const auto volume = mesh::analyze_volume(m);
            CHECK(!volume.supported() && volume.reason.find("non-manifold edges") != std::string::npos);
        }},
        {"analytic tetrahedron geometry", [] {
            const auto m = tetrahedron();
            near(mesh::surface_area(m), 1.5 + std::sqrt(3.0) / 2);
            near(mesh::signed_volume(m), 1.0 / 6);
            const auto t = mesh::topology(m);
            CHECK(t.closed_consistently_oriented && t.edges == 6 && t.euler_characteristic == 2);
            CHECK(t.connected_components == 1);
        }},
        {"analytic cube geometry", [] {
            const auto m = cube();
            near(mesh::surface_area(m), 6);
            near(mesh::signed_volume(m), 1);
            const auto t = mesh::topology(m);
            CHECK(t.closed_consistently_oriented && t.edges == 18 && t.euler_characteristic == 2);
            const auto normals = mesh::vertex_normals(m);
            CHECK(normals.size() == m.vertices.size());
            for (const auto n : normals) near(mesh::length(n), 1);
        }},
        {"volume winding and translated components", [] {
            auto m = tetrahedron();
            for (auto& t : m.triangles) std::swap(t[0], t[1]);
            near(mesh::signed_volume(m), -1.0 / 6);
            for (auto& v : m.vertices) v = v + mesh::Vec3{1e9, -1e9, 1e9};
            near(mesh::signed_volume(m), -1.0 / 6);
            std::swap(m.triangles[0][0], m.triangles[0][1]);
            CHECK(!mesh::topology(m).closed_consistently_oriented);
            throws<std::invalid_argument>([&] { (void)mesh::signed_volume(m); });
        }},
        {"disconnected components and isolated vertices", [] {
            auto m = tetrahedron();
            const auto other = tetrahedron();
            for (const auto v : other.vertices) m.vertices.push_back(v + mesh::Vec3{1e12, 0, 0});
            for (auto t : other.triangles) {
                for (auto& v : t) v += 4;
                m.triangles.push_back(t);
            }
            m.vertices.push_back({99, 99, 99});
            const auto topo = mesh::topology(m);
            CHECK(topo.connected_components == 3 && topo.isolated_vertices == 1);
            CHECK(topo.euler_characteristic == 5 && topo.closed_consistently_oriented);
            CHECK(!mesh::analyze_volume(m).supported());
            throws<std::invalid_argument>([&] { (void)mesh::signed_volume(m); });
            near(mesh::vertex_normals(m).back(), {});
        }},
        {"vertex normals use area weights and handle cancellation", [] {
            mesh::Mesh m{{{0, 0, 0}, {2, 0, 0}, {0, 2, 0}, {0, 0, 1}}, {{0, 1, 2}, {0, 3, 1}}};
            near(mesh::vertex_normals(m)[0], mesh::normalized({0, 2, 4}));
            m = {{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {-1, 0, 0}, {0, -1, 0}}, {{0, 1, 2}, {0, 4, 3}}};
            near(mesh::vertex_normals(m)[0], {});
        }},
        {"fixed-seed geometric scaling and translation properties", [] {
            std::mt19937 random(20261020);
            std::uniform_real_distribution<double> scale_distribution(0.01, 100);
            std::uniform_real_distribution<double> translation(-1000, 1000);
            for (int sample = 0; sample < 100; ++sample) {
                auto m = tetrahedron();
                const double scale = scale_distribution(random);
                const mesh::Vec3 offset{translation(random), translation(random), translation(random)};
                for (auto& v : m.vertices) v = v * scale + offset;
                near(mesh::surface_area(m), (1.5 + std::sqrt(3.0) / 2) * scale * scale, 1e-10);
                near(mesh::signed_volume(m), scale * scale * scale / 6, 1e-10);
                CHECK(mesh::topology(m).euler_characteristic == 2);
                for (const auto n : mesh::face_normals(m)) near(mesh::length(n), 1);
            }
        }},
        {"OBJ whitespace comments and metadata", [] {
            const auto m = parse("# Header\r\no object\ng group\ns off\nmtllib missing.mtl\nusemtl material\n"
                                 "\tv 0 0 0 # origin\r\nv 1 0 0\nv 0 1 0\nf 1 2 3 # triangle");
            CHECK(m.vertices == triangle().vertices && m.triangles == triangle().triangles);
            CHECK(parse("# empty\n\n").vertices.empty());
        }},
        {"OBJ relative and explicit positive indices", [] {
            CHECK(parse(positions + "f -3 -2 -1\n").triangles == triangle().triangles);
            CHECK(parse(positions + "f +1 +2 +3\n").triangles == triangle().triangles);
        }},
        {"OBJ homogeneous vertices and exponent notation", [] {
            const auto m = parse("v 0 0 0 2\nv +2e0 0 0 2\nv 0 2 0 2\nf 1 2 3\n");
            CHECK(m.vertices == triangle().vertices);
        }},
        {"OBJ texture and normal references", [] {
            const std::string attributes = "vt 0\nvt 0 1\nvt 0 1 0\nvn 0 0 1\n";
            for (const auto& face : {"f 1/1 2/2 3/3\n", "f 1//1 2//1 3//1\n", "f 1/1/1 2/2/1 3/3/1\n",
                                     "f -3/-3/-1 -2/-2/-1 -1/-1/-1\n"}) {
                CHECK(parse(positions + attributes + face).triangles == triangle().triangles);
            }
        }},
        {"OBJ invalid integer references", [] {
            for (const auto& face : {"f 0 2 3", "f 1 2 4", "f -4 -2 -1", "f 1.0 2 3", "f 1x 2 3",
                                     "f ++1 2 3", "f +-1 2 3", "f 9223372036854775808 2 3",
                                     "f -9223372036854775808 2 3"}) {
                throws<mesh::ObjError>([&] { (void)parse(positions + face); });
            }
        }},
        {"OBJ malformed slash references", [] {
            const std::string prefix = positions + "vt 0 0\nvn 0 0 1\n";
            for (const auto& face : {"f 1/ 2/1 3/1", "f 1// 2//1 3//1", "f /1 2/1 3/1",
                                     "f 1///1 2//1 3//1", "f 1/2 2/1 3/1", "f 1//2 2//1 3//1",
                                     "f 1/1/ 2/1/1 3/1/1"}) {
                throws<mesh::ObjError>([&] { (void)parse(prefix + face); });
            }
        }},
        {"OBJ rejects malformed polygons and unsupported geometry", [] {
            for (const auto& record : {"f 1 2", "f 1 2 3 1", "l 1 2", "p 1", "curv 0 1 1 2 3", "garbage 1"}) {
                throws<mesh::ObjError>([&] { (void)parse(positions + record); });
            }
        }},
        {"OBJ rejects malformed numeric fields", [] {
            for (const auto& record : {"v 0 0", "v 0 0 0 1 2", "v nan 0 0", "v inf 0 0", "v 1e309 0 0",
                                     "v 1x 0 0", "v +-1 0 0", "v ++1 0 0", "v + 0 0", "v 1 0 0 0",
                                     "v 1 0 0 1e-310", "vt", "vt 1 2 3 4", "vn 0 1", "vn 0 0 nan"}) {
                throws<mesh::ObjError>([&] { (void)parse(record); });
            }
        }},
        {"OBJ validation errors retain source line", [] {
            bool caught = false;
            try { (void)parse(positions + "# comment\nf 1 1 2\n"); }
            catch (const mesh::ObjError& error) {
                CHECK(error.line() == 5);
                CHECK(std::string(error.what()).find("OBJ line 5") != std::string::npos);
                caught = true;
            }
            CHECK(caught);
            throws<mesh::ObjError>([&] { (void)parse(positions + "f 1 2 3\nf 3 2 1\n"); });
        }},
        {"OBJ exact floating-point round trip", [] {
            auto m = tetrahedron();
            m.vertices[1].x = 1.2345678901234567;
            m.vertices.push_back({1e-200, -1e200, std::numeric_limits<double>::denorm_min()});
            std::ostringstream out;
            mesh::write_obj(out, m);
            const auto loaded = parse(out.str());
            CHECK(loaded.vertices == m.vertices);
            CHECK(loaded.triangles == m.triangles);
        }},
        {"OBJ round trips randomized closed meshes", [] {
            std::mt19937 random(42);
            std::uniform_real_distribution<double> distribution(-100, 100);
            for (int i = 0; i < 50; ++i) {
                auto m = cube();
                const mesh::Vec3 translation{distribution(random), distribution(random), distribution(random)};
                for (auto& v : m.vertices) v = v + translation;
                std::ostringstream output;
                mesh::write_obj(output, m);
                const auto loaded = parse(output.str());
                CHECK(loaded.vertices == m.vertices && loaded.triangles == m.triangles);
                near(mesh::signed_volume(loaded), 1);
            }
        }},
        {"OBJ writer preserves stream formatting and locale", [] {
            std::ostringstream output;
            output.imbue(std::locale(std::locale::classic(), new CommaDecimal));
            output << std::fixed << std::setprecision(2);
            const auto flags = output.flags();
            mesh::write_obj(output, triangle());
            CHECK(output.flags() == flags && output.precision() == 2);
            CHECK(std::use_facet<std::numpunct<char>>(output.getloc()).decimal_point() == ',');
            CHECK(parse(output.str()).vertices == triangle().vertices);
        }},
        {"OBJ writer leaves output untouched for invalid meshes", [] {
            auto m = triangle();
            m.triangles[0][0] = 99;
            std::ostringstream output;
            output << "preserve";
            throws<std::invalid_argument>([&] { mesh::write_obj(output, m); });
            CHECK(output.str() == "preserve");
        }},
        {"convex quad triangulation preserves winding", [] {
            const auto m = parse("v 0 0 0\nv 2 0 0\nv 2 3 0\nv 0 3 0\nf 1 2 3 4\n");
            CHECK(m.triangles.size() == 2);
            CHECK(mesh::face_normals(m).size() == 2);
            near(mesh::surface_area(m), 6);
            for (const auto n : mesh::face_normals(m)) near(n, {0, 0, 1});
            const auto reversed = parse("v 0 0 0\nv 2 0 0\nv 2 3 0\nv 0 3 0\nf -1 -2 -3 -4\n");
            near(mesh::surface_area(reversed), 6);
            for (const auto n : mesh::face_normals(reversed)) near(n, {0, 0, -1});
        }},
        {"polygons in arbitrary planes with slash syntax", [] {
            const auto m = parse("v -2 -1 -3\nv -2 1 -3\nv -2 1 -1\nv -2 -1 -1\n"
                                 "vt 0\nvn 1 0 0\nf 1/1/1 2/1/1 3/1/1 4/1/1\n");
            CHECK(m.triangles.size() == 2);
            near(mesh::surface_area(m), 4);
            near(mesh::face_normals(m)[0], {1, 0, 0});
            const auto tilted = parse("v -3 -4 -5\nv -2 -3 -5\nv -3 -2 -3\nv -4 -3 -3\nf 1 2 3 4\n");
            CHECK(tilted.triangles.size() == 2);
            near(mesh::surface_area(tilted), 2 * std::sqrt(3.0));
            near(mesh::face_normals(tilted).at(0), {1/std::sqrt(3.0),-1/std::sqrt(3.0),1/std::sqrt(3.0)});
        }},
        {"convex pentagons and stars", [] {
            std::vector<mesh::Vec3> vertices;
            for (int i = 0; i < 5; ++i) {
                const auto angle = static_cast<double>(i) * 2 * std::acos(-1.0) / 5;
                vertices.push_back({std::cos(angle), std::sin(angle), 0});
            }
            const std::vector<std::size_t> boundary{0, 1, 2, 3, 4}, star{0, 2, 4, 1, 3};
            mesh::Mesh m{vertices, mesh::triangulate_convex_polygon(vertices, boundary)};
            CHECK(m.triangles.size() == 3);
            near(mesh::surface_area(m), 2.5 * std::sin(2 * std::acos(-1.0) / 5));
            throws<std::invalid_argument>([&] { (void)mesh::triangulate_convex_polygon(vertices, star); });
        }},
        {"reject nonplanar concave crossing and collinear polygons", [] {
            const std::vector<std::string> invalid{
                "v 0 0 0\nv 2 0 0\nv 2 2 0\nv 0 2 1\nf 1 2 3 4",
                "v 0 0 0\nv 2 0 0\nv 1 0.5 0\nv 2 2 0\nv 0 2 0\nf 1 2 3 4 5",
                "v 0 0 0\nv 2 2 0\nv 0 2 0\nv 2 0 0\nf 1 2 3 4",
                "v 0 0 0\nv 1 0 0\nv 2 0 0\nv 2 2 0\nv 0 2 0\nf 1 2 3 4 5",
                "v 0 0 0\nv 2 0 0\nv 2 2 0\nv 0 0 0\nf 1 2 3 4"
            };
            for (const auto& text : invalid) throws<mesh::ObjError>([&] { (void)parse(text); });
        }},
        {"triangulation API validates inputs", [] {
            const auto m = triangle();
            for (const auto& indices : {std::vector<std::size_t>{}, {0, 1}, {0, 1, 8}, {0, 1, 1}}) {
                throws<std::invalid_argument>([&] { (void)mesh::triangulate_convex_polygon(m.vertices, indices); });
            }
            auto vertices = m.vertices;
            vertices[1].x = std::numeric_limits<double>::quiet_NaN();
            throws<std::invalid_argument>([&] { (void)mesh::triangulate_convex_polygon(vertices, m.triangles[0]); });
        }},
        {"polygon validation source lines and scale invariance", [] {
            const std::string bad = "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 1\nf 1 2 3 4\n";
            bool caught = false;
            try { (void)parse(bad); }
            catch (const mesh::ObjError& error) { CHECK(error.line() == 5); caught = true; }
            CHECK(caught);
            for (const double scale : {1e-100, 1.0, 1e100}) {
                std::vector<mesh::Vec3> v{{0, 0, 0}, {scale, 0, 0}, {scale, scale, 0}, {0, scale, 0}};
                const std::vector<std::size_t> p{0, 1, 2, 3};
                CHECK(mesh::triangulate_convex_polygon(v, p).size() == 2);
            }
        }},
        {"AABB positive and negative dimensions", [] {
            const mesh::Mesh m{{{-9, -8, -7}, {-2, -3, -4}, {5, 6, 7}}, {}};
            const auto box = mesh::bounds(m);
            near(box->min, {-9, -8, -7});
            near(box->max, {5, 6, 7});
            near(box->width(), 14);
            near(box->height(), 14);
            near(box->depth(), 14);
            const mesh::Mesh negative{{{-9, -8, -7}, {-2, -3, -4}}, {}};
            near(mesh::bounds(negative)->max, {-2, -3, -4});
        }},
        {"enclosed volume normalizes inward orientation", [] {
            auto m = cube();
            for (auto& t : m.triangles) std::swap(t[0], t[1]);
            const auto result = mesh::analyze_volume(m);
            CHECK(result.supported() && result.reason.empty());
            near(*result.signed_value, -1);
            near(*result.enclosed_value, 1);
        }},
        {"nonmanifold vertex links block volume", [] {
            auto m = tetrahedron();
            const auto other = tetrahedron();
            // Two closed tetrahedra glued at vertex 0; every edge has two faces.
            m.vertices.insert(m.vertices.end(), {{-1, 0, 0}, {0, -1, 0}, {0, 0, -1}});
            for (auto t : other.triangles) {
                for (auto& i : t) if (i != 0) i += 3;
                std::swap(t[0], t[1]);
                m.triangles.push_back(t);
            }
            const auto t = mesh::topology(m);
            CHECK(t.nonmanifold_edges == 0 && t.nonmanifold_vertices == 1);
            CHECK(!t.closed_consistently_oriented);
            CHECK(!mesh::analyze_volume(m).supported());
        }},
        {"open meshes retain area without a volume", [] {
            auto m = cube();
            m.triangles.resize(10);
            near(mesh::surface_area(m), 5);
            const auto result = mesh::analyze_volume(m);
            CHECK(!result.supported() && !result.enclosed_value);
            CHECK(result.reason.find("open") != std::string::npos);
        }},
        {"isolated vertices do not change supported surface volume", [] {
            auto m = tetrahedron();
            m.vertices.push_back({1e6, 1e6, 1e6});
            CHECK(mesh::topology(m).surface_components == 1);
            near(mesh::signed_volume(m), 1.0 / 6);
        }},
        {"self-crossing closed surface is rejected", [] {
            auto m = cube();
            m.vertices[6] = {0.25, 0.25, -0.5};
            CHECK(mesh::topology(m).closed_consistently_oriented);
            const auto result = mesh::analyze_volume(m);
            CHECK(!result.supported());
            CHECK(result.reason.find("intersection") != std::string::npos);
        }},
        {"coincident surface positions with distinct indices rejected", [] {
            auto m = cube();
            m.vertices[6] = {0, 0, 0};
            CHECK(mesh::validate(m).empty());
            CHECK(!mesh::analyze_volume(m).supported());
        }},
        {"flat closed tetrahedron has no supported enclosed volume", [] {
            auto m = tetrahedron();
            m.vertices[3] = {0.25, 0.25, 0};
            CHECK(mesh::validate(m).empty());
            CHECK(!mesh::analyze_volume(m).supported());
        }},
        {"volume overflow and underflow are unavailable", [] {
            for (const double scale : {1e-110, 1e110}) {
                auto m = tetrahedron();
                for (auto& p : m.vertices) p = p * scale;
                CHECK(mesh::validate(m).empty());
                CHECK(!mesh::analyze_volume(m).supported());
            }
        }},
        {"nonconvex closed L prism analytical results", [] {
            const auto m = l_prism();
            CHECK(mesh::topology(m).closed_consistently_oriented);
            near(mesh::surface_area(m), 14);
            near(mesh::signed_volume(m), 3);
            near(mesh::analyze_volume(m).enclosed_value.value(), 3);
        }},
        {"rigid rotation preserves volume and intersection eligibility", [] {
            for (int sample = 1; sample <= 30; ++sample) {
                auto m = l_prism();
                const double angle = static_cast<double>(sample) * 0.173;
                const auto c = std::cos(angle), s = std::sin(angle);
                for (auto& p : m.vertices) {
                    p = {c * p.x - s * p.y, s * p.x + c * p.y, p.z};
                    p = {p.x, c * p.y - s * p.z, s * p.y + c * p.z};
                }
                near(mesh::surface_area(m), 14);
                near(mesh::signed_volume(m), 3);
            }
        }},
        {"smallest area underflow is rejected", [] {
            const mesh::Mesh m{{{0, 0, 0}, {1, 0, 0}, {0, std::numeric_limits<double>::denorm_min(), 0}}, {{0, 1, 2}}};
            CHECK(has_issue(m, mesh::IssueCode::degenerate_triangle));
            throws<std::invalid_argument>([&] { (void)mesh::surface_area(m); });
        }},
        {"OBJ writer ignores and preserves width fill and alignment", [] {
            for (const auto alignment : {std::ios::left, std::ios::right, std::ios::internal}) {
                std::ostringstream output;
                output.setf(alignment, std::ios::adjustfield);
                output << std::setfill('x') << std::setw(200);
                const auto flags = output.flags();
                mesh::write_obj(output, triangle());
                CHECK(output.width() == 200 && output.fill() == 'x' && output.flags() == flags);
                const auto loaded = parse(output.str());
                CHECK(loaded.vertices == triangle().vertices && loaded.triangles == triangle().triangles);
            }
        }},
        {"OBJ accepts ordinary EOF with all stream exception masks", [] {
            for (const auto mask : {std::ios::goodbit, std::ios::eofbit, std::ios::failbit, std::ios::badbit,
                                   std::ios::eofbit | std::ios::failbit, std::ios::eofbit | std::ios::badbit,
                                   std::ios::failbit | std::ios::badbit,
                                   std::ios::eofbit | std::ios::failbit | std::ios::badbit}) {
                for (const auto& text : {std::string{}, positions + "f 1 2 3", positions + "f 1 2 3\n"}) {
                    std::istringstream input(text);
                    input.exceptions(mask);
                    const auto loaded = mesh::read_obj(input);
                    CHECK(input.exceptions() == mask);
                    CHECK(loaded.vertices.size() == (text.empty() ? 0U : 3U));
                    if (!text.empty()) CHECK(loaded.triangles == triangle().triangles);
                }
            }
        }},
        {"OBJ rejects prefailed EOF and interrupted valid input", [] {
            for (const auto state : {std::ios::eofbit, std::ios::failbit | std::ios::eofbit, std::ios::badbit}) {
                std::istringstream input(positions);
                input.setstate(state);
                throws<std::runtime_error>([&] { (void)mesh::read_obj(input); });
            }
            for (const auto mask : {std::ios::goodbit, std::ios::badbit}) {
                InterruptedInput buffer;
                std::istream input(&buffer);
                input.exceptions(mask);
                throws<std::runtime_error>([&] { (void)mesh::read_obj(input); });
                CHECK(input.bad() && input.exceptions() == mask);
            }
        }},
        {"OBJ reference counts are independent and resolved at each face", [] {
            const auto loaded = parse(positions + "vt 0 0\nvn 0 0 1\nf -3/-1/-1 -2/-1/-1 -1/-1/-1\n"
                                      "v 0 0 1\nf -3//-1 -2//-1 -1//-1\n");
            CHECK((loaded.triangles == std::vector<mesh::Triangle>{{0,1,2},{1,2,3}}));
            throws<mesh::ObjError>([&] { (void)parse(positions + "vn 0 0 1\nf 1/1/1 2/1/1 3/1/1"); });
            throws<mesh::ObjError>([&] { (void)parse("f 1 2 3\n" + positions); });
        }},
        {"OBJ duplicate diagnostics survive polygon expansion", [] {
            bool caught = false;
            try { (void)parse("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\nf 3 2 1\n"); }
            catch (const mesh::ObjError& error) {
                CHECK(error.line() == 6 && std::string(error.what()).find("duplicates") != std::string::npos);
                caught = true;
            }
            CHECK(caught);
        }},
        {"aggregate area and normal overflow are reported", [] {
            mesh::Mesh m;
            for (std::size_t i = 0; i < 4; ++i) {
                m.vertices.insert(m.vertices.end(), {{0,0,0},{1e154,0,0},{0,1e154,0}});
                m.triangles.push_back({3*i,3*i+1,3*i+2});
            }
            CHECK(mesh::validate(m).empty());
            throws<std::overflow_error>([&] { (void)mesh::surface_area(m); });
            m = {{{0,0,0},{1e154,0,0},{0,1e154,0},{0,1.1e154,0}},{{0,1,2},{0,1,3}}};
            CHECK(mesh::validate(m).empty());
            throws<std::overflow_error>([&] { (void)mesh::vertex_normals(m); });
        }},
        {"representable extreme area and volume use relative checks", [] {
            for (const double scale : {1e-50, 1e-20, 1e20, 1e80}) {
                auto m = tetrahedron();
                for (auto& p : m.vertices) p = p * scale;
                relative_near(mesh::surface_area(m), (1.5 + std::sqrt(3.0)/2) * scale * scale);
                relative_near(mesh::signed_volume(m), scale * scale * scale / 6);
            }
        }},
        {"closed genus one ring has analytical volume and topology", [] {
            const auto m = square_ring();
            const auto t = mesh::topology(m);
            CHECK(t.closed_consistently_oriented && t.euler_characteristic == 0);
            CHECK(t.surface_components == 1 && t.nonmanifold_vertices == 0);
            near(mesh::surface_area(m), 48);
            near(mesh::signed_volume(m), 12);
        }},
        {"nested and overlapping components have no enclosed volume", [] {
            for (const double scale : {0.5, 1.0}) {
                auto m = cube();
                const auto other = cube();
                for (const auto p : other.vertices) m.vertices.push_back(p * scale + mesh::Vec3{0.25,0.25,0.25});
                for (auto t : other.triangles) {
                    for (auto& i : t) i += 8;
                    m.triangles.push_back(t);
                }
                const auto result = mesh::analyze_volume(m);
                CHECK(!result.signed_value && !result.enclosed_value);
                CHECK(result.reason.find("Multiple surface components") != std::string::npos);
            }
        }},
        {"open pinched vertex is detected without nonmanifold edges", [] {
            const mesh::Mesh m{{{0,0,0},{1,0,0},{0,1,0},{-1,0,0},{0,-1,0}},{{0,1,2},{0,3,4}}};
            const auto t = mesh::topology(m);
            CHECK(t.nonmanifold_edges == 0 && t.nonmanifold_vertices == 1 && t.boundary_edges == 6);
            CHECK(!mesh::analyze_volume(m).supported());
            near(mesh::surface_area(m), 1);
        }},
        {"OBJ stream failures are reported", [] {
            BrokenInput input_buffer;
            std::istream input(&input_buffer);
            throws([&] { (void)mesh::read_obj(input); });
            BrokenOutput output_buffer;
            std::ostream output(&output_buffer);
            throws([&] { mesh::write_obj(output, triangle()); });
            std::istringstream failed_input(positions);
            failed_input.setstate(std::ios::failbit);
            throws([&] { (void)mesh::read_obj(failed_input); });
        }},
        {"OBJ byte and line limits include comments and EOF", [] {
            const auto text = positions + "f 1 2 3";
            mesh::ObjReadLimits limits;
            limits.max_bytes = text.size();
            std::istringstream exact(text);
            CHECK(mesh::read_obj(exact, limits).triangles.size() == 1);
            --limits.max_bytes;
            std::istringstream too_large(text);
            throws<mesh::ObjError>([&] { (void)mesh::read_obj(too_large, limits); });
            limits = {};
            limits.max_line_bytes = 7;
            std::istringstream exact_lines(text);
            CHECK(mesh::read_obj(exact_lines, limits).triangles.size() == 1);
            limits.max_line_bytes = 6;
            std::istringstream long_line(text);
            throws<mesh::ObjError>([&] { (void)mesh::read_obj(long_line, limits); });
            std::istringstream comment("#" + std::string(100, 'x'));
            throws<mesh::ObjError>([&] { (void)mesh::read_obj(comment, limits); });
        }},
        {"OBJ position and attribute counts are bounded", [] {
            mesh::ObjReadLimits limits;
            limits.max_vertices = 2;
            std::istringstream vertices(positions);
            throws<mesh::ObjError>([&] { (void)mesh::read_obj(vertices, limits); });
            limits.max_vertices = 3;
            limits.max_attribute_records = 1;
            std::istringstream attributes(positions + "vt 0\nvn 0 0 1\n");
            throws<mesh::ObjError>([&] { (void)mesh::read_obj(attributes, limits); });
            limits.max_attribute_records = 2;
            std::istringstream exact(positions + "vt 0\nvn 0 0 1\nf 1/1/1 2/1/1 3/1/1\n");
            CHECK(mesh::read_obj(exact, limits).triangles.size() == 1);
            limits.max_vertices = 0;
            std::istringstream empty;
            CHECK(mesh::read_obj(empty, limits).vertices.empty());
        }},
        {"OBJ face and emitted triangle budgets are bounded", [] {
            const std::string quad = "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n";
            mesh::ObjReadLimits limits;
            limits.max_face_vertices = 3;
            std::istringstream corners(quad);
            throws<mesh::ObjError>([&] { (void)mesh::read_obj(corners, limits); });
            limits.max_face_vertices = 4;
            limits.max_triangles = 1;
            std::istringstream triangles(quad);
            throws<mesh::ObjError>([&] { (void)mesh::read_obj(triangles, limits); });
            limits.max_triangles = 2;
            std::istringstream exact(quad);
            CHECK(mesh::read_obj(exact, limits).triangles.size() == 2);
        }},
        {"volume pair budget has an inclusive safe boundary", [] {
            const auto m = tetrahedron(); // 4 choose 2 = 6 pairs.
            CHECK(mesh::analyze_volume(m, 6).supported());
            const auto exceeded = mesh::analyze_volume(m, 5);
            CHECK(!exceeded.signed_value && !exceeded.enclosed_value);
            CHECK(exceeded.reason == "Intersection check budget exceeded");
            CHECK(mesh::analyze_volume(m, std::numeric_limits<std::size_t>::max()).supported());
            near(mesh::surface_area(m), 1.5 + std::sqrt(3.0)/2);
        }},
        {"OBJ diagnostics do not echo untrusted control sequences", [] {
            bool rejected = false;
            try { (void)parse(std::string("\x1b[31m") + std::string(200, 'x')); }
            catch (const mesh::ObjError& error) {
                const std::string message = error.what();
                CHECK(message.find('\x1b') == std::string::npos && message.size() < 100);
                rejected = true;
            }
            CHECK(rejected);
        }}
    };
    std::size_t failures{};
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "FAIL " << name << ": unknown exception\n";
        }
    }
    std::cout << tests.size() - failures << '/' << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
