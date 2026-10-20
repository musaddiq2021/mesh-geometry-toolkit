#pragma once

#include "mesh/geometry.hpp"

#include <filesystem>
#include <iosfwd>
#include <stdexcept>
#include <string>

namespace mesh {

/// @brief Invalid/unsupported OBJ input, including configured resource limits.
class ObjError : public std::runtime_error {
public:
    ObjError(std::size_t line, const std::string& message);
    [[nodiscard]] std::size_t line() const noexcept { return line_; }
private:
    std::size_t line_;
};

/// @brief Bounds applied before growing line, position, attribute or face buffers.
/// Trusted callers may supply different limits; zero permits none of that data.
struct ObjReadLimits {
    std::size_t max_bytes = 32 * 1024 * 1024;
    std::size_t max_line_bytes = 64 * 1024;
    std::size_t max_vertices = 250'000;
    std::size_t max_triangles = 250'000;
    std::size_t max_attribute_records = 250'000;
    std::size_t max_face_vertices = 256;
};

/**
 * @brief Read triangle/planar strictly convex faces and preserve their winding.
 * Positive indices are one-based; negatives refer to elements defined at that
 * face line. Slash attributes are validated then discarded. Input must initially
 * be good; normal EOF is accepted without changing stream exception masks.
 * @throws ObjError For malformed/unsupported records, geometry or exceeded limits.
 * @throws std::runtime_error For I/O failures; no partial mesh is returned.
 * @throws std::bad_alloc If allocation fails within configured limits.
 */
[[nodiscard]] Mesh read_obj(std::istream& input, const ObjReadLimits& limits = {});
/// @brief Open in binary mode (including on Windows), then apply read_obj().
[[nodiscard]] Mesh load_obj(const std::filesystem::path& path, const ObjReadLimits& limits = {});
/// @brief Write at round-trip precision, preserving caller stream formatting.
/// @throws std::invalid_argument Before writing invalid geometry; runtime_error on I/O failure.
void write_obj(std::ostream& output, const Mesh& mesh);
/**
 * @brief Stage complete output beside the destination, then replace its directory entry.
 * Existing output is replaced only after validation, writing and closing succeed.
 * Uses exclusive temporary creation; failures attempt to remove only that temporary.
 * No cross-platform atomicity or crash-durability guarantee is made. The caller
 * must control the destination directory; this is not a filesystem sandbox.
 * @throws std::exception For invalid geometry, allocation or filesystem failure.
 */
void save_obj(const std::filesystem::path& path, const Mesh& mesh);

} // namespace mesh
