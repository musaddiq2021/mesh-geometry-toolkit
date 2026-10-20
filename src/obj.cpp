#include "mesh/obj.hpp"
#include "mesh/triangulation.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <random>
#include <sstream>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace mesh {
namespace {

std::string_view without_plus(std::string_view text) {
    if (!text.empty() && text.front() == '+') text.remove_prefix(1);
    // Prevent accepting malformed double signs after stripping a plus.
    if (text.empty() || text.front() == '+') return {};
    return text;
}

double number(std::string_view text, std::size_t line) {
    const bool plus = !text.empty() && text.front() == '+';
    text = without_plus(text);
    if (text.empty() || (plus && text.front() == '-')) throw ObjError(line, "Expected a finite number");
    double value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{}
        || parsed.ptr != text.data() + text.size() || !std::isfinite(value)) {
        throw ObjError(line, "Expected a finite number");
    }
    return value;
}

std::size_t index(std::string_view text, std::size_t count, std::size_t line) {
    const bool plus = !text.empty() && text.front() == '+';
    text = without_plus(text);
    if (text.empty() || (plus && text.front() == '-')) {
        throw ObjError(line, "Face references must be nonzero integer indices");
    }
    std::int64_t value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{}
        || parsed.ptr != text.data() + text.size() || value == 0) {
        throw ObjError(line, "Face references must be nonzero integer indices");
    }
    if (value > 0) {
        const auto position = static_cast<std::uint64_t>(value - 1);
        if (position >= count) throw ObjError(line, "Face index references an undefined element");
        return static_cast<std::size_t>(position);
    }
    // Avoid negating INT64_MIN.
    const auto distance = static_cast<std::uint64_t>(-(value + 1)) + 1;
    if (distance > count) throw ObjError(line, "Relative face index references an undefined element");
    return count - static_cast<std::size_t>(distance);
}

std::size_t face_vertex(std::string_view text, std::size_t vertices,
                        std::size_t textures, std::size_t normals, std::size_t line) {
    const auto first = text.find('/');
    if (first == std::string_view::npos) return index(text, vertices, line);
    const auto vertex = index(text.substr(0, first), vertices, line);
    const auto second = text.find('/', first + 1);
    if (second == std::string_view::npos) {
        (void)index(text.substr(first + 1), textures, line);
        return vertex;
    }
    if (text.find('/', second + 1) != std::string_view::npos) {
        throw ObjError(line, "Too many '/' separators in face reference");
    }
    if (second != first + 1) (void)index(text.substr(first + 1, second - first - 1), textures, line);
    (void)index(text.substr(second + 1), normals, line);
    return vertex;
}

bool read_line(std::istream& input, std::string& line, const ObjReadLimits& limits,
               std::size_t& bytes, std::size_t line_number) {
    line.clear();
    for (;;) {
        std::istream::int_type character;
        try { character = input.get(); }
        catch (const std::ios_base::failure&) {
            if (input.bad() || !input.eof()) throw;
            character = std::istream::traits_type::eof();
        }
        if (std::istream::traits_type::eq_int_type(character, std::istream::traits_type::eof())) return !line.empty();
        if (bytes == limits.max_bytes) throw ObjError(line_number, "OBJ byte limit exceeded");
        ++bytes;
        const char value = std::istream::traits_type::to_char_type(character);
        if (value == '\n') return true;
        if (line.size() == limits.max_line_bytes) throw ObjError(line_number, "OBJ line length limit exceeded");
        line.push_back(value);
    }
}

// Own only an exclusively created sibling file. Never open/truncate the target.
// Publishing uses rename after a complete write/close; directory control remains
// the caller's responsibility (no protection against hostile parent-directory swaps).
class StagedOutput {
public:
    explicit StagedOutput(const std::filesystem::path& destination) : destination_(destination) {
        const auto parent = destination.parent_path().empty() ? std::filesystem::path{"."} : destination.parent_path();
        std::random_device random;
        for (unsigned attempt = 0; attempt < 16; ++attempt) {
            std::ostringstream name;
            name << ".mesh-output-" << std::hex << random() << random() << random() << random() << ".tmp";
            temporary_ = parent / name.str();
#ifdef _WIN32
            handle_ = CreateFileW(temporary_.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle_ != INVALID_HANDLE_VALUE) return;
            const auto error = GetLastError();
            if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
                throw std::system_error(static_cast<int>(error), std::system_category(), "Cannot create temporary OBJ output");
            }
#else
            descriptor_ = ::open(temporary_.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
            if (descriptor_ >= 0) return;
            if (errno != EEXIST) throw std::system_error(errno, std::generic_category(), "Cannot create temporary OBJ output");
#endif
        }
        throw std::runtime_error("Cannot reserve a temporary OBJ output name");
    }
    StagedOutput(const StagedOutput&) = delete;
    StagedOutput& operator=(const StagedOutput&) = delete;
    ~StagedOutput() {
#ifdef _WIN32
        if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
#else
        if (descriptor_ >= 0) ::close(descriptor_);
#endif
        if (!published_) {
            try {
                std::error_code ignored;
                std::filesystem::remove(temporary_, ignored);
            } catch (...) { /* Cleanup must not mask an earlier I/O/allocation error. */ }
        }
    }
    void write(std::string_view text) {
        while (!text.empty()) {
#ifdef _WIN32
            const auto count = static_cast<DWORD>(std::min<std::size_t>(text.size(), 1024 * 1024));
            DWORD written{};
            if (!WriteFile(handle_, text.data(), count, &written, nullptr) || written == 0) {
                throw std::runtime_error("Failed while writing temporary OBJ output");
            }
#else
            const auto written = ::write(descriptor_, text.data(), std::min<std::size_t>(text.size(), 1024 * 1024));
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) throw std::runtime_error("Failed while writing temporary OBJ output");
#endif
            text.remove_prefix(static_cast<std::size_t>(written));
        }
    }
    void publish() {
#ifdef _WIN32
        if (!FlushFileBuffers(handle_)) throw std::runtime_error("Failed while flushing temporary OBJ output");
        const bool closed = CloseHandle(handle_) != 0;
        if (closed) handle_ = INVALID_HANDLE_VALUE;
#else
        if (::fsync(descriptor_) != 0) throw std::runtime_error("Failed while flushing temporary OBJ output");
        const bool closed = ::close(descriptor_) == 0;
        descriptor_ = -1;
#endif
        if (!closed) throw std::runtime_error("Failed while closing temporary OBJ output");
        std::filesystem::rename(temporary_, destination_);
        published_ = true;
    }
private:
    std::filesystem::path destination_, temporary_;
    bool published_{};
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int descriptor_ = -1;
#endif
};

} // namespace

ObjError::ObjError(std::size_t line, const std::string& message)
    : std::runtime_error("OBJ line " + std::to_string(line) + ": " + message), line_(line) {}

Mesh read_obj(std::istream& input, const ObjReadLimits& limits) {
    if (!input.good()) throw std::runtime_error("OBJ input stream is not ready for reading");
    Mesh result;
    std::size_t texture_count{}, normal_count{}, attribute_count{}, line_number{}, bytes{};
    std::vector<std::size_t> triangle_lines;
    std::string line;
    while (read_line(input, line, limits, bytes, line_number + 1)) {
        ++line_number;
        if (const auto comment = line.find('#'); comment != std::string::npos) line.resize(comment);
        std::istringstream tokens(line);
        tokens.imbue(std::locale::classic());
        std::string keyword;
        if (!(tokens >> keyword)) continue;
        std::vector<std::string> fields;
        for (std::string field; tokens >> field;) fields.push_back(field);
        if (keyword == "v") {
            if (result.vertices.size() == limits.max_vertices) throw ObjError(line_number, "OBJ vertex count limit exceeded");
            if (fields.size() != 3 && fields.size() != 4) throw ObjError(line_number, "Vertex requires x y z and optional w");
            Vec3 vertex{number(fields[0], line_number), number(fields[1], line_number), number(fields[2], line_number)};
            if (fields.size() == 4) {
                const double weight = number(fields[3], line_number);
                if (weight == 0.0) throw ObjError(line_number, "Homogeneous vertex weight cannot be zero");
                vertex = vertex / weight;
            }
            if (!is_finite(vertex)) throw ObjError(line_number, "Vertex coordinates exceed double precision range");
            result.vertices.push_back(vertex);
        } else if (keyword == "vt" || keyword == "vn") {
            if (attribute_count == limits.max_attribute_records) throw ObjError(line_number, "OBJ attribute count limit exceeded");
            ++attribute_count;
            if ((keyword == "vn" && fields.size() != 3)
                || (keyword == "vt" && (fields.empty() || fields.size() > 3))) {
                throw ObjError(line_number, "Invalid texture coordinate or normal component count");
            }
            for (const auto& field : fields) (void)number(field, line_number);
            if (keyword == "vt") ++texture_count;
            else ++normal_count;
        } else if (keyword == "f") {
            if (fields.size() > limits.max_face_vertices) throw ObjError(line_number, "OBJ face corner limit exceeded");
            std::vector<std::size_t> polygon;
            polygon.reserve(fields.size());
            for (const auto& field : fields) {
                polygon.push_back(face_vertex(field, result.vertices.size(), texture_count, normal_count, line_number));
            }
            try {
                const auto triangles = triangulate_convex_polygon(result.vertices, polygon);
                if (triangles.size() > limits.max_triangles - result.triangles.size()) {
                    throw ObjError(line_number, "OBJ triangle count limit exceeded");
                }
                result.triangles.insert(result.triangles.end(), triangles.begin(), triangles.end());
                triangle_lines.insert(triangle_lines.end(), triangles.size(), line_number);
            } catch (const std::invalid_argument& error) {
                throw ObjError(line_number, error.what());
            }
        } else if (keyword != "o" && keyword != "g" && keyword != "s"
                   && keyword != "usemtl" && keyword != "mtllib") {
            // Do not echo terminal control sequences or an oversized attacker token.
            throw ObjError(line_number, "Unsupported OBJ record");
        }
    }
    if (input.bad() || !input.eof()) throw std::runtime_error("Failed while reading OBJ stream");
    const auto issues = validate(result);
    if (!issues.empty()) {
        const auto& issue = issues.front();
        throw ObjError(triangle_lines.at(issue.element), issue.message);
    }
    return result;
}

Mesh load_obj(const std::filesystem::path& path, const ObjReadLimits& limits) {
    // Binary input keeps Windows text mode from treating Ctrl-Z as a hidden EOF.
    // CRLF remains whitespace for the line/token parser.
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open OBJ input: " + path.string());
    return read_obj(input, limits);
}

void write_obj(std::ostream& output, const Mesh& m) {
    require_valid(m);
    // A private stream preserves the caller's locale, formatting and precision.
    std::ostringstream text;
    text.exceptions(std::ios::badbit | std::ios::failbit);
    text.imbue(std::locale::classic());
    text << std::setprecision(std::numeric_limits<double>::max_digits10);
    text << "# Mesh Geometry Toolkit\n";
    for (const auto v : m.vertices) text << "v " << v.x << ' ' << v.y << ' ' << v.z << '\n';
    for (const auto& t : m.triangles) text << "f " << t[0] + 1 << ' ' << t[1] + 1 << ' ' << t[2] + 1 << '\n';
    const auto serialized = text.view();
    if (serialized.size() > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::length_error("OBJ output exceeds the stream size range");
    }
    // Unformatted output ignores width/fill/alignment and preserves them.
    output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
    if (!output) throw std::runtime_error("Failed while writing OBJ stream");
}

void save_obj(const std::filesystem::path& path, const Mesh& m) {
    std::ostringstream serialized;
    write_obj(serialized, m); // Validation/serialization finish before creating a file.
    StagedOutput output(path);
    output.write(serialized.view());
    output.publish();
}

} // namespace mesh
