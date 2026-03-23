#include "balsa/geometry/point_cloud/extxyz.hpp"
#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <ranges>
#include <sstream>
#include <stdexcept>

namespace balsa::geometry::point_cloud {

// ── Helper utilities ─────────────────────────────────────────────────────

namespace {

    // Trim leading/trailing whitespace.
    std::string trim(std::string_view sv) {
        while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front())))
            sv.remove_prefix(1);
        while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back())))
            sv.remove_suffix(1);
        return std::string(sv);
    }

    // Parse a double from a string_view using from_chars.
    double parse_double(std::string_view sv) {
        double val = 0.0;
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
        if (ec != std::errc{}) {
            throw std::runtime_error(std::format("Failed to parse double from '{}'",
                                                 std::string(sv)));
        }
        return val;
    }

    // Parse an int from a string_view.
    int parse_int(std::string_view sv) {
        int val = 0;
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
        if (ec != std::errc{}) {
            throw std::runtime_error(std::format("Failed to parse int from '{}'",
                                                 std::string(sv)));
        }
        return val;
    }

    // Parse a bool from a token: T/True/1 -> true, F/False/0 -> false.
    bool parse_bool(std::string_view sv) {
        auto s = trim(sv);
        if (s == "T" || s == "True" || s == "true" || s == "1") return true;
        if (s == "F" || s == "False" || s == "false" || s == "0") return false;
        throw std::runtime_error(std::format("Failed to parse bool from '{}'", s));
    }

    // Convert ExtXYZType to its single-char representation.
    char type_char(ExtXYZType t) {
        switch (t) {
        case ExtXYZType::String:
            return 'S';
        case ExtXYZType::Integer:
            return 'I';
        case ExtXYZType::Real:
            return 'R';
        case ExtXYZType::Logical:
            return 'L';
        }
        return '?';
    }

    // Parse a type character to ExtXYZType.
    ExtXYZType parse_type_char(char c) {
        switch (c) {
        case 'S':
            return ExtXYZType::String;
        case 'I':
            return ExtXYZType::Integer;
        case 'R':
            return ExtXYZType::Real;
        case 'L':
            return ExtXYZType::Logical;
        default:
            throw std::runtime_error(std::format("Unknown extxyz property type '{}'", c));
        }
    }

    // Split a string by a delimiter, returning string_view tokens.
    std::vector<std::string_view> split(std::string_view sv, char delim) {
        std::vector<std::string_view> result;
        size_t start = 0;
        while (start <= sv.size()) {
            auto pos = sv.find(delim, start);
            if (pos == std::string_view::npos) {
                result.push_back(sv.substr(start));
                break;
            }
            result.push_back(sv.substr(start, pos - start));
            start = pos + 1;
        }
        return result;
    }

    // Tokenize a data line into whitespace-separated tokens.
    std::vector<std::string_view> tokenize_line(std::string_view line) {
        std::vector<std::string_view> tokens;
        size_t i = 0;
        while (i < line.size()) {
            // Skip whitespace.
            while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
                ++i;
            if (i >= line.size()) break;
            size_t start = i;
            while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i])))
                ++i;
            tokens.push_back(line.substr(start, i - start));
        }
        return tokens;
    }

    // Parse a 3x3 lattice matrix from a string of 9 whitespace-separated numbers.
    // The extxyz convention stores lattice vectors as rows:
    //   "a1x a1y a1z a2x a2y a2z a3x a3y a3z"
    // We return the matrix in row-major form (row i = lattice vector i).
    balsa::Mat3d parse_lattice(const std::string &s) {
        auto tokens = tokenize_line(s);
        if (tokens.size() != 9) {
            throw std::runtime_error(
              std::format("Lattice must have 9 values, got {}", tokens.size()));
        }
        balsa::Mat3d mat;
        for (int i = 0; i < 9; ++i) {
            mat(i / 3, i % 3) = parse_double(tokens[i]);
        }
        return mat;
    }

    // Format a 3x3 lattice matrix as a string.
    std::string format_lattice(const balsa::Mat3d &mat) {
        return std::format("{:.10g} {:.10g} {:.10g} {:.10g} {:.10g} {:.10g} {:.10g} {:.10g} {:.10g}",
                           mat(0, 0),
                           mat(0, 1),
                           mat(0, 2),
                           mat(1, 0),
                           mat(1, 1),
                           mat(1, 2),
                           mat(2, 0),
                           mat(2, 1),
                           mat(2, 2));
    }

    // Parse pbc string: "T T F" or "T T T" etc.
    std::array<bool, 3> parse_pbc(const std::string &s) {
        auto tokens = tokenize_line(s);
        if (tokens.size() != 3) {
            throw std::runtime_error(
              std::format("pbc must have 3 values, got {}", tokens.size()));
        }
        return { parse_bool(tokens[0]), parse_bool(tokens[1]), parse_bool(tokens[2]) };
    }

}// namespace

// ── Comment line parsing ─────────────────────────────────────────────────

bool is_extxyz_comment(const std::string &line) {
    // Look for Properties= (case-sensitive, as per convention).
    return line.find("Properties=") != std::string::npos;
}

ParsedCommentLine parse_extxyz_comment(const std::string &line) {
    ParsedCommentLine result;

    // The comment line is a series of key=value pairs.
    // Values can be:
    //   - Quoted with double quotes: key="value with spaces"
    //   - Unquoted: key=value (no spaces)
    // Keys are always unquoted.
    //
    // We parse left to right, handling quoted values.
    size_t i = 0;
    const size_t n = line.size();

    while (i < n) {
        // Skip whitespace.
        while (i < n && std::isspace(static_cast<unsigned char>(line[i])))
            ++i;
        if (i >= n) break;

        // Find '='.
        size_t eq_pos = line.find('=', i);
        if (eq_pos == std::string::npos) {
            // Rest of line is a bare key with no value (or trailing text).
            // Store it with empty value.
            auto key = trim(line.substr(i));
            if (!key.empty()) {
                result.info[key] = "";
            }
            break;
        }

        std::string key = trim(line.substr(i, eq_pos - i));
        i = eq_pos + 1;

        // Parse value.
        std::string value;
        if (i < n && line[i] == '"') {
            // Quoted value — find closing quote.
            ++i;
            size_t close = line.find('"', i);
            if (close == std::string::npos) {
                // No closing quote — take rest of line.
                value = line.substr(i);
                i = n;
            } else {
                value = line.substr(i, close - i);
                i = close + 1;
            }
        } else {
            // Unquoted value — take until whitespace.
            size_t start = i;
            while (i < n && !std::isspace(static_cast<unsigned char>(line[i])))
                ++i;
            value = line.substr(start, i - start);
        }

        result.info[key] = value;
    }

    // Parse the Properties value if present.
    if (auto it = result.info.find("Properties"); it != result.info.end()) {
        auto &props_str = it->second;
        // Properties format: name:type:cols[:name:type:cols:...]
        auto parts = split(props_str, ':');
        if (parts.size() % 3 != 0) {
            throw std::runtime_error(
              std::format("Properties string '{}' must have fields in groups of 3 (name:type:cols)",
                          props_str));
        }
        for (size_t j = 0; j + 2 < parts.size(); j += 3) {
            ExtXYZPropertyDef def;
            def.name = std::string(parts[j]);
            if (parts[j + 1].size() != 1) {
                throw std::runtime_error(
                  std::format("Property type must be a single character, got '{}'",
                              std::string(parts[j + 1])));
            }
            def.type = parse_type_char(parts[j + 1][0]);
            def.cols = parse_int(parts[j + 2]);
            result.property_defs.push_back(std::move(def));
        }
        // Remove Properties from info map — it's stored separately.
        result.info.erase(it);
    }

    return result;
}

// ── Properties string building ───────────────────────────────────────────

std::string build_properties_string(const std::vector<ExtXYZPropertyDef> &defs) {
    std::string s;
    for (size_t i = 0; i < defs.size(); ++i) {
        if (i > 0) s += ':';
        s += std::format("{}:{}:{}", defs[i].name, type_char(defs[i].type), defs[i].cols);
    }
    return s;
}

std::string build_extxyz_comment(const std::map<std::string, std::string> &info,
                                 const std::vector<ExtXYZPropertyDef> &defs) {
    std::string line;

    // Build the output, starting with Lattice (if present) and pbc,
    // then Properties, then remaining keys.
    auto write_kv = [&](const std::string &key, const std::string &value) {
        if (!line.empty()) line += ' ';
        // Quote values that contain spaces.
        if (value.find(' ') != std::string::npos) {
            line += std::format("{}=\"{}\"", key, value);
        } else {
            line += std::format("{}={}", key, value);
        }
    };

    // Lattice first (convention).
    if (auto it = info.find("Lattice"); it != info.end()) {
        write_kv("Lattice", it->second);
    }

    // Properties next.
    write_kv("Properties", build_properties_string(defs));

    // pbc next if present.
    if (auto it = info.find("pbc"); it != info.end()) {
        write_kv("pbc", it->second);
    }

    // Remaining keys in sorted order.
    for (const auto &[key, value] : info) {
        if (key == "Lattice" || key == "pbc") continue;
        write_kv(key, value);
    }

    return line;
}

// ── ExtXYZFrame accessors ────────────────────────────────────────────────

const ExtXYZProperty *ExtXYZFrame::property(const std::string &name) const {
    for (const auto &[n, p] : properties) {
        if (n == name) return &p;
    }
    return nullptr;
}

ExtXYZProperty *ExtXYZFrame::property(const std::string &name) {
    for (auto &[n, p] : properties) {
        if (n == name) return &p;
    }
    return nullptr;
}

balsa::ColVecs3d ExtXYZFrame::positions() const {
    const auto *p = property("pos");
    if (!p || p->type != ExtXYZType::Real || p->cols != 3 || !p->real_data) {
        throw std::runtime_error("ExtXYZFrame has no 'pos' property (R:3)");
    }
    // real_data is (3 x N), which matches ColVecs3d.
    balsa::ColVecs3d result(3, atom_count);
    for (size_t i = 0; i < atom_count; ++i) {
        for (int r = 0; r < 3; ++r) {
            result(r, i) = (*p->real_data)(r, i);
        }
    }
    return result;
}

std::vector<std::string> ExtXYZFrame::species() const {
    const auto *p = property("species");
    if (!p || p->type != ExtXYZType::String || !p->string_data) {
        return {};
    }
    return *p->string_data;
}

std::vector<std::string> ExtXYZFrame::property_names() const {
    std::vector<std::string> names;
    names.reserve(properties.size());
    for (const auto &[name, _] : properties) {
        names.push_back(name);
    }
    return names;
}

// ── Reader ───────────────────────────────────────────────────────────────

namespace {

    // Read a single frame from an open stream.  Returns false if no more
    // frames are available.
    bool read_frame(std::istream &is, ExtXYZFrame &frame) {
        std::string line;

        // Skip blank lines between frames.
        while (std::getline(is, line)) {
            auto trimmed = trim(line);
            if (!trimmed.empty()) break;
        }
        if (is.eof() || line.empty()) return false;

        // Line 1: atom count.
        frame.atom_count = static_cast<size_t>(parse_int(trim(line)));

        // Line 2: comment/info line.
        if (!std::getline(is, line)) {
            throw std::runtime_error("Unexpected end of file reading comment line");
        }

        auto parsed = parse_extxyz_comment(line);
        frame.info = std::move(parsed.info);

        // If no Properties definition was found, use default: species:S:1:pos:R:3
        auto &prop_defs = parsed.property_defs;
        if (prop_defs.empty()) {
            prop_defs = {
                { "species", ExtXYZType::String, 1 },
                { "pos", ExtXYZType::Real, 3 },
            };
        }

        // Compute total expected columns.
        int total_cols = 0;
        for (const auto &def : prop_defs) {
            total_cols += def.cols;
        }

        // Allocate storage for each property.
        for (const auto &def : prop_defs) {
            ExtXYZProperty prop;
            prop.type = def.type;
            prop.cols = def.cols;

            switch (def.type) {
            case ExtXYZType::Real:
                prop.real_data.emplace(def.cols, frame.atom_count);
                break;
            case ExtXYZType::Integer:
                prop.int_data.emplace(def.cols, frame.atom_count);
                break;
            case ExtXYZType::String:
                prop.string_data.emplace(frame.atom_count * def.cols);
                break;
            case ExtXYZType::Logical:
                prop.bool_data.emplace();
                prop.bool_data->resize(frame.atom_count * def.cols);
                break;
            }

            frame.properties.emplace_back(def.name, std::move(prop));
        }

        // Read atom data lines.
        for (size_t atom = 0; atom < frame.atom_count; ++atom) {
            if (!std::getline(is, line)) {
                throw std::runtime_error(
                  std::format("Unexpected end of file at atom {}/{}", atom, frame.atom_count));
            }

            auto tokens = tokenize_line(line);
            if (static_cast<int>(tokens.size()) < total_cols) {
                throw std::runtime_error(
                  std::format("Line {} has {} columns, expected {}",
                              atom + 3,
                              tokens.size(),
                              total_cols));
            }

            int tok_idx = 0;
            for (size_t pi = 0; pi < frame.properties.size(); ++pi) {
                auto &[name, prop] = frame.properties[pi];
                for (int c = 0; c < prop.cols; ++c) {
                    auto tok = tokens[tok_idx++];
                    switch (prop.type) {
                    case ExtXYZType::Real:
                        (*prop.real_data)(c, atom) = parse_double(tok);
                        break;
                    case ExtXYZType::Integer:
                        (*prop.int_data)(c, atom) = parse_int(tok);
                        break;
                    case ExtXYZType::String:
                        (*prop.string_data)[atom * prop.cols + c] = std::string(tok);
                        break;
                    case ExtXYZType::Logical:
                        (*prop.bool_data)[atom * prop.cols + c] = parse_bool(tok);
                        break;
                    }
                }
            }
        }

        // Extract Lattice if present in info.
        if (auto it = frame.info.find("Lattice"); it != frame.info.end()) {
            frame.lattice = parse_lattice(it->second);
        }

        // Extract pbc if present.
        if (auto it = frame.info.find("pbc"); it != frame.info.end()) {
            frame.pbc = parse_pbc(it->second);
        }

        return true;
    }

}// namespace

std::vector<ExtXYZFrame> read_extxyz(const std::string &filename) {
    std::ifstream ifs(filename);
    if (!ifs) {
        throw std::runtime_error("Failed to open extxyz file: " + filename);
    }

    std::vector<ExtXYZFrame> frames;
    while (true) {
        ExtXYZFrame frame;
        if (!read_frame(ifs, frame)) break;
        frames.push_back(std::move(frame));
    }

    if (frames.empty()) {
        throw std::runtime_error("No frames found in extxyz file: " + filename);
    }
    return frames;
}

ExtXYZFrame read_extxyz_frame(const std::string &filename) {
    std::ifstream ifs(filename);
    if (!ifs) {
        throw std::runtime_error("Failed to open extxyz file: " + filename);
    }

    ExtXYZFrame frame;
    if (!read_frame(ifs, frame)) {
        throw std::runtime_error("No frames found in extxyz file: " + filename);
    }
    return frame;
}

// ── Writer ───────────────────────────────────────────────────────────────

namespace {

    void write_frame(std::ostream &os, const ExtXYZFrame &frame) {
        // Line 1: atom count.
        os << frame.atom_count << '\n';

        // Build property definitions from the frame's properties.
        std::vector<ExtXYZPropertyDef> defs;
        for (const auto &[name, prop] : frame.properties) {
            defs.push_back({ name, prop.type, prop.cols });
        }

        // Build the info map for the comment line.
        auto info = frame.info;

        // If lattice is set, serialize it into info.
        if (frame.lattice) {
            info["Lattice"] = format_lattice(*frame.lattice);
        }

        // If pbc is set, serialize it.
        if (frame.pbc) {
            auto &p = *frame.pbc;
            info["pbc"] = std::format("{} {} {}",
                                      p[0] ? "T" : "F",
                                      p[1] ? "T" : "F",
                                      p[2] ? "T" : "F");
        }

        // Line 2: comment line.
        os << build_extxyz_comment(info, defs) << '\n';

        // Data lines.
        for (size_t atom = 0; atom < frame.atom_count; ++atom) {
            bool first = true;
            for (const auto &[name, prop] : frame.properties) {
                for (int c = 0; c < prop.cols; ++c) {
                    if (!first) os << ' ';
                    first = false;
                    switch (prop.type) {
                    case ExtXYZType::Real:
                        os << std::format("{:.10g}", (*prop.real_data)(c, atom));
                        break;
                    case ExtXYZType::Integer:
                        os << (*prop.int_data)(c, atom);
                        break;
                    case ExtXYZType::String:
                        os << (*prop.string_data)[atom * prop.cols + c];
                        break;
                    case ExtXYZType::Logical:
                        os << ((*prop.bool_data)[atom * prop.cols + c] ? "T" : "F");
                        break;
                    }
                }
            }
            os << '\n';
        }
    }

}// namespace

void write_extxyz(const std::string &filename, const std::vector<ExtXYZFrame> &frames) {
    std::ofstream ofs(filename);
    if (!ofs) {
        throw std::runtime_error("Failed to open extxyz file for writing: " + filename);
    }

    for (const auto &frame : frames) {
        write_frame(ofs, frame);
    }
}

void write_extxyz(const std::string &filename, const ExtXYZFrame &frame) {
    write_extxyz(filename, std::vector<ExtXYZFrame>{ frame });
}

}// namespace balsa::geometry::point_cloud
