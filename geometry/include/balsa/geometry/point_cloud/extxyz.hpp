#if !defined(BALSA_GEOMETRY_POINT_CLOUD_EXTXYZ_HPP)
#define BALSA_GEOMETRY_POINT_CLOUD_EXTXYZ_HPP

// Extended XYZ (extxyz) file format reader/writer.
//
// The Extended XYZ format adds structured metadata to the standard XYZ format:
//   Line 1: <atom_count>
//   Line 2: key1=value1 key2="value 2" Properties=species:S:1:pos:R:3:forces:R:3 ...
//   Lines 3..N+2: per-atom columns as described by Properties
//
// The comment line is parsed as key=value pairs.  Two keys have special
// meaning:
//   - Properties  Describes per-atom column layout:  name:type:cols[:name:type:cols:...]
//                 Types: S=string, I=integer, R=real, L=logical (bool)
//   - Lattice     Nine numbers defining the 3x3 cell matrix (row-major):
//                 "a1x a1y a1z a2x a2y a2z a3x a3y a3z"
//
// References:
//   - ASE wiki: https://wiki.fysik.dtu.dk/ase/ase/io/formatoptions.html#extxyz
//   - libAtoms/extxyz specification

#include "balsa/tensor_types.hpp"
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace balsa::geometry::point_cloud {

// ── Property column descriptors ──────────────────────────────────────────

/// Type tag for a single property column group in the Properties string.
enum class ExtXYZType {
    String,///< S — string column(s)
    Integer,///< I — integer column(s)
    Real,///< R — real (double) column(s)
    Logical,///< L — logical (bool) column(s)
};

/// One entry from the Properties specification, e.g. "forces:R:3".
struct ExtXYZPropertyDef {
    std::string name;
    ExtXYZType type;
    int cols;///< Number of columns occupied (e.g. 3 for a 3-vector)
};

// ── Per-atom property storage ────────────────────────────────────────────

/// A named per-atom property with its data.
/// Each variant holds a column-major matrix or a string vector.
///
/// For multi-column real properties (e.g. positions, forces), the data is
/// stored as a ColVectors<double, dynamic_extent> with shape (cols x N).
/// Single-column reals are stored as VecXd.
/// Integers similarly.  Strings and logicals as vectors.
struct ExtXYZProperty {
    ExtXYZType type;
    int cols;///< Number of columns per atom

    // Storage — exactly one of these is populated:
    std::optional<balsa::ColVectors<double, std::dynamic_extent>> real_data;///< (cols x N) for R
    std::optional<balsa::ColVectors<int, std::dynamic_extent>> int_data;///< (cols x N) for I
    std::optional<std::vector<std::string>> string_data;///< N strings for S
    std::optional<std::vector<bool>> bool_data;///< N bools for L
};

// ── Frame data ───────────────────────────────────────────────────────────

/// Data from a single Extended XYZ frame.
struct ExtXYZFrame {
    size_t atom_count = 0;

    /// Per-atom properties, keyed by property name.
    /// Insertion order is preserved (matches column order in the file).
    std::vector<std::pair<std::string, ExtXYZProperty>> properties;

    /// Key-value info from the comment line (everything except Properties=).
    std::map<std::string, std::string> info;

    /// The Lattice matrix (3x3, row-major in the file).  Populated from
    /// info["Lattice"] if present.
    std::optional<balsa::Mat3d> lattice;

    /// Whether periodic boundary conditions are active per axis.
    /// Populated from info["pbc"] if present.
    std::optional<std::array<bool, 3>> pbc;

    // ── Convenience accessors ────────────────────────────────────────

    /// Get a property by name, or nullptr if not present.
    const ExtXYZProperty *property(const std::string &name) const;
    ExtXYZProperty *property(const std::string &name);

    /// Get positions (the "pos" property) as a 3xN double matrix.
    /// Throws if "pos" is not present or is not a 3-column real property.
    balsa::ColVecs3d positions() const;

    /// Get species (the "species" property) as a string vector.
    /// Returns empty vector if not present.
    std::vector<std::string> species() const;

    /// List all property names in column order.
    std::vector<std::string> property_names() const;
};

// ── Reader ───────────────────────────────────────────────────────────────

/// Read all frames from an Extended XYZ file.
std::vector<ExtXYZFrame> read_extxyz(const std::string &filename);

/// Read a single frame (the first) from an Extended XYZ file.
ExtXYZFrame read_extxyz_frame(const std::string &filename);

// ── Writer ───────────────────────────────────────────────────────────────

/// Write frames to an Extended XYZ file.
void write_extxyz(const std::string &filename,
                  const std::vector<ExtXYZFrame> &frames);

/// Write a single frame to an Extended XYZ file.
void write_extxyz(const std::string &filename,
                  const ExtXYZFrame &frame);

// ── Comment line parsing utilities ───────────────────────────────────────

/// Parse the comment (second) line of an XYZ file into key-value pairs.
/// Returns the map and also extracts the Properties definition list.
struct ParsedCommentLine {
    std::map<std::string, std::string> info;
    std::vector<ExtXYZPropertyDef> property_defs;
};

ParsedCommentLine parse_extxyz_comment(const std::string &line);

/// Check if a comment line looks like Extended XYZ (contains "Properties=").
bool is_extxyz_comment(const std::string &line);

/// Build a Properties string from property definitions.
std::string build_properties_string(const std::vector<ExtXYZPropertyDef> &defs);

/// Build a comment line from info map and property definitions.
std::string build_extxyz_comment(const std::map<std::string, std::string> &info,
                                 const std::vector<ExtXYZPropertyDef> &defs);

}// namespace balsa::geometry::point_cloud

#endif
