#include "mesh/geometry.hpp"
#include "mesh/obj.hpp"

#include <exception>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <string_view>

namespace {

void usage(std::ostream& out) {
    out << "Mesh Geometry Toolkit\n"
           "Usage:\n"
           "  mesh-tool inspect INPUT.obj\n"
           "  mesh-tool convert INPUT.obj OUTPUT.obj\n"
           "  mesh-tool --help\n"
           "  mesh-tool --version\n"
           "Supports triangles and planar strictly convex OBJ polygons.\n"
           "Conversion retains positions and triangulated faces.\n";
}

void inspect(const mesh::Mesh& m) {
    const auto topo = mesh::topology(m);
    const auto box = mesh::bounds(m);
    const double area = mesh::surface_area(m);
    const auto volume = mesh::analyze_volume(m);
    std::cout.imbue(std::locale::classic());
    std::cout << std::setprecision(17)
              << "Vertices: " << m.vertices.size() << '\n'
              << "Triangles: " << m.triangles.size() << '\n'
              << "Edges: " << topo.edges << '\n'
              << "Boundary edges: " << topo.boundary_edges << '\n'
              << "Nonmanifold edges: " << topo.nonmanifold_edges << '\n'
              << "Nonmanifold vertices: " << topo.nonmanifold_vertices << '\n'
              << "Inconsistent winding edges: " << topo.inconsistent_winding_edges << '\n'
              << "Connected components: " << topo.connected_components << '\n'
              << "Surface components: " << topo.surface_components << '\n'
              << "Isolated vertices: " << topo.isolated_vertices << '\n'
              << "Euler characteristic: " << topo.euler_characteristic << '\n'
              << "Closed with consistent edge winding: " << (topo.closed_consistently_oriented ? "yes" : "no") << '\n'
              << "Surface area: " << area << '\n';
    if (box) {
        std::cout << "Bounds min: " << box->min.x << ' ' << box->min.y << ' ' << box->min.z << '\n'
                  << "Bounds max: " << box->max.x << ' ' << box->max.y << ' ' << box->max.z << '\n';
        const auto dimension = [](const char* label, double value) {
            std::cout << label << ": ";
            if (std::isfinite(value)) std::cout << value << '\n';
            else std::cout << "unavailable (dimension exceeds double precision range)\n";
        };
        dimension("Width", box->width());
        dimension("Height", box->height());
        dimension("Depth", box->depth());
    } else {
        std::cout << "Bounds: empty\n";
    }
    if (volume.supported()) {
        std::cout << "Signed volume: " << *volume.signed_value << '\n'
                  << "Enclosed volume: " << *volume.enclosed_value << '\n';
    } else {
        std::cout << "Enclosed volume: unavailable (" << volume.reason << ")\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--version") {
            std::cout << "Mesh Geometry Toolkit 1.0.0\n";
        } else if (argc == 2 && std::string_view(argv[1]) == "--help") {
            usage(std::cout);
        } else {
            const bool inspect_command = argc == 3 && std::string_view(argv[1]) == "inspect";
            const bool convert_command = argc == 4 && std::string_view(argv[1]) == "convert";
            if (!inspect_command && !convert_command) {
                usage(std::cerr);
                return 2;
            }
            const auto m = mesh::load_obj(argv[2]);
            if (inspect_command) inspect(m);
            else {
                if (std::filesystem::exists(argv[3]) && std::filesystem::equivalent(argv[2], argv[3])) {
                    throw std::invalid_argument("Input and output must refer to different files");
                }
                mesh::save_obj(argv[3], m);
                std::cout << "Wrote " << m.vertices.size() << " vertices and " << m.triangles.size() << " triangles\n";
            }
        }
        std::cout.flush();
        if (!std::cout) throw std::runtime_error("Failed while writing command output");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: ";
        // Filenames in filesystem diagnostics can contain terminal controls.
        for (const char character : std::string_view(error.what())) {
            const auto byte = static_cast<unsigned char>(character);
            std::cerr.put(byte < 32 || byte == 127 ? '?' : character);
        }
        std::cerr << '\n';
        return 1;
    }
}
