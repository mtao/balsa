// particle_converter — Unified particle file conversion tool for balsa.
//
// Converts particle data between XYZ, Partio (.bgeo/.geo/.pdb/...) and
// optionally OpenVDB (.vdb) formats.
//
// Usage:
//   particle_converter <input> <output> [options]
//
// Format is inferred from file extensions.  VDB support requires compilation
// with -DBALSA_HAS_OPENVDB=1.

#include <CLI/CLI.hpp>
#include <balsa/geometry/point_cloud/read_xyz.hpp>
#include <balsa/geometry/point_cloud/write_xyz.hpp>
#include <balsa/geometry/point_cloud/partio_loader.hpp>
#include <format>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <map>
#include <set>

#if BALSA_HAS_OPENVDB
#include <balsa/geometry/point_cloud/vdb_points.hpp>
#endif

namespace pc = balsa::geometry::point_cloud;

// ── Format detection ─────────────────────────────────────────────────────

enum class Format { XYZ,
                    Partio,
                    VDB,
                    Unknown };

Format detect_format(const std::string &path) {
    auto ext = std::filesystem::path(path).extension().string();
    std::ranges::transform(ext, ext.begin(), ::tolower);
    if (ext == ".xyz") return Format::XYZ;
    if (ext == ".vdb") return Format::VDB;
    // Everything else goes to Partio — it supports .bgeo, .geo, .pdb,
    // .bhclassic, .pda, .pdc, etc.
    return Format::Partio;
}

std::string format_name(Format f) {
    switch (f) {
    case Format::XYZ:
        return "XYZ";
    case Format::Partio:
        return "Partio";
    case Format::VDB:
        return "VDB";
    default:
        return "Unknown";
    }
}

// ── Intermediate particle representation ─────────────────────────────────

struct ParticleData {
    balsa::ColVecs3d positions;
    std::optional<balsa::ColVecs3d> velocities;
    std::optional<balsa::ColVecs4d> colors;
    std::optional<balsa::VecXd> radii;
    std::optional<balsa::VecXd> densities;
    std::optional<balsa::VecXi> ids;
    std::optional<std::vector<std::string>> species;
    std::optional<balsa::ColVecs3d> extra;// XYZ second column block
    std::string comment;// XYZ comment line

    std::vector<std::string> available_attributes() const {
        std::vector<std::string> attrs;
        attrs.push_back("positions");
        if (velocities) attrs.push_back("velocities");
        if (colors) attrs.push_back("colors");
        if (radii) attrs.push_back("radii");
        if (densities) attrs.push_back("densities");
        if (ids) attrs.push_back("ids");
        if (species) attrs.push_back("species");
        if (extra) attrs.push_back("extra");
        return attrs;
    }

    size_t particle_count() const {
        return positions.cols();
    }
};

// ── Built-in attribute name mappings ─────────────────────────────────────
// Maps between internal canonical names and format-specific names.

// Internal name -> VDB name
static const std::map<std::string, std::string> s_internal_to_vdb = {
    { "radii", "pscale" },
    { "velocities", "v" },
    { "colors", "Cd" },
    { "densities", "density" },
    { "ids", "id" },
};

// VDB name -> internal name
static const std::map<std::string, std::string> s_vdb_to_internal = {
    { "pscale", "radii" },
    { "v", "velocities" },
    { "Cd", "colors" },
    { "density", "densities" },
    { "id", "ids" },
};

// ── Readers ──────────────────────────────────────────────────────────────

ParticleData read_xyz_file(const std::string &path) {
    auto xyz = pc::read_xyzD(path);
    ParticleData data;
    data.positions = std::move(xyz.positions);
    data.comment = std::move(xyz.comment);
    if (!xyz.species.empty()) {
        data.species = std::move(xyz.species);
    }
    if (xyz.extra.cols() > 0) {
        data.extra = std::move(xyz.extra);
    }
    return data;
}

ParticleData read_partio_file(const std::string &path) {
    pc::PartioFileReader reader(path);
    ParticleData data;
    data.positions = reader.positions();
    if (reader.has_velocities()) data.velocities = reader.velocities();
    if (reader.has_colors()) data.colors = reader.colors();
    if (reader.has_radii()) data.radii = reader.radii();
    if (reader.has_densities()) data.densities = reader.densities();
    if (reader.has_ids()) data.ids = reader.ids();
    return data;
}

#if BALSA_HAS_OPENVDB
ParticleData read_vdb_file(const std::string &path, const std::string &grid_name) {
    auto vdb_data = pc::vdb::read_vdb(path, grid_name);
    if (!vdb_data) {
        throw std::runtime_error("No PointDataGrid found in VDB file: " + path);
    }
    ParticleData data;
    data.positions = std::move(vdb_data->positions);
    if (vdb_data->velocities) data.velocities = std::move(vdb_data->velocities);
    if (vdb_data->colors) data.colors = std::move(vdb_data->colors);
    if (vdb_data->radii) data.radii = std::move(vdb_data->radii);
    if (vdb_data->densities) data.densities = std::move(vdb_data->densities);
    if (vdb_data->ids) data.ids = std::move(vdb_data->ids);
    return data;
}
#endif

// ── Writers ──────────────────────────────────────────────────────────────

void write_xyz_file(const std::string &path, const ParticleData &data) {
    pc::write_xyz(path,
                  data.positions,
                  data.species.value_or(std::vector<std::string>{}),
                  data.extra.value_or(balsa::ColVecs3d{}),
                  data.comment);
}

void write_partio_file(const std::string &path, const ParticleData &data) {
    pc::PartioFileWriter writer(path);
    writer.set_positions(data.positions);
    if (data.velocities) writer.set_velocities(*data.velocities);
    if (data.colors) writer.set_colors(*data.colors);
    if (data.radii) writer.set_radii(*data.radii);
    if (data.densities) writer.set_densities(*data.densities);
    if (data.ids) writer.set_ids(*data.ids);
}

#if BALSA_HAS_OPENVDB
void write_vdb_file(const std::string &path, const ParticleData &data, double voxel_size, const std::string &grid_name) {
    pc::vdb::VDBParticleData vdb_data;
    vdb_data.positions = data.positions;
    if (data.velocities) vdb_data.velocities = data.velocities;
    if (data.colors) vdb_data.colors = data.colors;
    if (data.radii) vdb_data.radii = data.radii;
    if (data.densities) vdb_data.densities = data.densities;
    if (data.ids) vdb_data.ids = data.ids;
    pc::vdb::write_vdb(path, vdb_data, voxel_size, grid_name);
}
#endif

// ── Attribute filtering ──────────────────────────────────────────────────

// Supported attributes for each output format.
static const std::set<std::string> s_xyz_attrs = { "positions", "species", "extra" };
static const std::set<std::string> s_partio_attrs = {
    "positions",
    "velocities",
    "colors",
    "radii",
    "densities",
    "ids"
};
static const std::set<std::string> s_vdb_attrs = {
    "positions",
    "velocities",
    "colors",
    "radii",
    "densities",
    "ids"
};

const std::set<std::string> &supported_attrs(Format f) {
    switch (f) {
    case Format::XYZ:
        return s_xyz_attrs;
    case Format::Partio:
        return s_partio_attrs;
    case Format::VDB:
        return s_vdb_attrs;
    default:
        return s_partio_attrs;
    }
}

void filter_attributes(ParticleData &data,
                       const std::vector<std::string> &whitelist,
                       Format output_format) {
    // Build the effective set of attributes to keep.
    // If whitelist is empty, keep all. Otherwise keep only those listed.
    auto out_supported = supported_attrs(output_format);

    auto should_keep = [&](const std::string &name) -> bool {
        if (!out_supported.contains(name)) return false;
        if (whitelist.empty()) return true;
        return std::ranges::find(whitelist, name) != whitelist.end();
    };

    // positions are always kept
    if (!should_keep("velocities")) data.velocities.reset();
    if (!should_keep("colors")) data.colors.reset();
    if (!should_keep("radii")) data.radii.reset();
    if (!should_keep("densities")) data.densities.reset();
    if (!should_keep("ids")) data.ids.reset();
    if (!should_keep("species")) data.species.reset();
    if (!should_keep("extra")) data.extra.reset();
}

// ── Attribute renaming ───────────────────────────────────────────────────

std::string apply_rename(const std::string &name,
                         const std::map<std::string, std::string> &user_renames,
                         Format from,
                         Format to) {
    // User renames take priority
    if (auto it = user_renames.find(name); it != user_renames.end()) {
        return it->second;
    }
    // Apply built-in mappings
    if (to == Format::VDB) {
        if (auto it = s_internal_to_vdb.find(name); it != s_internal_to_vdb.end()) {
            return it->second;
        }
    }
    if (from == Format::VDB) {
        if (auto it = s_vdb_to_internal.find(name); it != s_vdb_to_internal.end()) {
            return it->second;
        }
    }
    return name;
}

// ── Info printing ────────────────────────────────────────────────────────

void print_info(const std::string &path, Format format) {
    std::cout << std::format("File: {}\nFormat: {}\n", path, format_name(format));

    if (format == Format::XYZ) {
        auto data = read_xyz_file(path);
        std::cout << std::format("Particles: {}\n", data.particle_count());
        std::cout << "Attributes:\n";
        for (const auto &a : data.available_attributes()) {
            std::cout << "  - " << a << '\n';
        }
    } else if (format == Format::Partio) {
        pc::PartioFileReader reader(path);
        std::cout << std::format("Particles: {}\n", reader.particle_count());
        std::cout << "Attributes:\n";
        for (const auto &a : reader.attributes()) {
            std::cout << "  - " << a << '\n';
        }
    }
#if BALSA_HAS_OPENVDB
    else if (format == Format::VDB) {
        auto grids = pc::vdb::list_vdb_grids(path);
        std::cout << std::format("Grids ({}):\n", grids.size());
        for (const auto &[name, type] : grids) {
            std::cout << std::format("  - {} ({})\n", name, type);
        }
    }
#endif
}

// ── Main ─────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    CLI::App app{ "particle_converter — convert particle data between XYZ, Partio, and VDB formats" };

    std::string input_path;
    std::string output_path;
    std::vector<std::string> attr_whitelist;
    std::vector<std::string> renames_raw;
    std::string grid_name = "particles";
    double voxel_size = 0.1;
    bool info_mode = false;

    app.add_option("input", input_path, "Input particle file")
      ->required()
      ->check(CLI::ExistingFile);
    app.add_option("output", output_path, "Output particle file");

    app.add_option("-a,--attr", attr_whitelist,
                   "Attributes to transfer (default: all). "
                   "Can be repeated: -a positions -a velocities -a radii -a densities -a colors -a ids -a species -a extra");

    app.add_option("-r,--rename", renames_raw,
                   "Rename an attribute: -r src:dst. "
                   "Built-in mappings (pscale<->radii, v<->velocities, Cd<->colors) apply by default.");

    app.add_option("-n,--grid-name", grid_name, "VDB grid name for reading/writing (default: 'particles')");

    app.add_option("-v,--voxel-size", voxel_size, "Voxel size for VDB output (default: 0.1)");

    app.add_flag("-i,--info", info_mode, "Print information about the input file and exit");

    CLI11_PARSE(app, argc, argv);

    // Parse rename pairs
    std::map<std::string, std::string> user_renames;
    for (const auto &r : renames_raw) {
        auto sep = r.find(':');
        if (sep == std::string::npos) {
            std::cerr << std::format("Invalid rename '{}': expected format 'src:dst'\n", r);
            return 1;
        }
        user_renames[r.substr(0, sep)] = r.substr(sep + 1);
    }

    Format input_format = detect_format(input_path);

    // Info mode
    if (info_mode) {
        print_info(input_path, input_format);
        return 0;
    }

    // Output is required for conversion
    if (output_path.empty()) {
        std::cerr << "Output path is required for conversion (use --info / -i for file info)\n";
        return 1;
    }

    Format output_format = detect_format(output_path);

    // VDB guard
#if !BALSA_HAS_OPENVDB
    if (input_format == Format::VDB || output_format == Format::VDB) {
        std::cerr << "VDB support not compiled. Rebuild with -Dopenvdb=true\n";
        return 1;
    }
#endif

    // ── Read ─────────────────────────────────────────────────────────
    ParticleData data;
    try {
        switch (input_format) {
        case Format::XYZ:
            data = read_xyz_file(input_path);
            break;
        case Format::Partio:
            data = read_partio_file(input_path);
            break;
#if BALSA_HAS_OPENVDB
        case Format::VDB:
            data = read_vdb_file(input_path, grid_name);
            break;
#endif
        default:
            std::cerr << "Unsupported input format\n";
            return 1;
        }
    } catch (const std::exception &e) {
        std::cerr << std::format("Error reading {}: {}\n", input_path, e.what());
        return 1;
    }

    std::cout << std::format("Read {} particles from {} ({})\n",
                             data.particle_count(),
                             input_path,
                             format_name(input_format));

    // ── Report available attributes ──────────────────────────────────
    auto available = data.available_attributes();
    std::cout << "Available attributes:";
    for (const auto &a : available) std::cout << " " << a;
    std::cout << '\n';

    // ── Filter attributes ────────────────────────────────────────────
    filter_attributes(data, attr_whitelist, output_format);

    auto transferring = data.available_attributes();
    std::cout << "Transferring attributes:";
    for (const auto &a : transferring) std::cout << " " << a;
    std::cout << '\n';

    // ── Write ────────────────────────────────────────────────────────
    try {
        switch (output_format) {
        case Format::XYZ:
            write_xyz_file(output_path, data);
            break;
        case Format::Partio:
            write_partio_file(output_path, data);
            break;
#if BALSA_HAS_OPENVDB
        case Format::VDB:
            write_vdb_file(output_path, data, voxel_size, grid_name);
            break;
#endif
        default:
            std::cerr << "Unsupported output format\n";
            return 1;
        }
    } catch (const std::exception &e) {
        std::cerr << std::format("Error writing {}: {}\n", output_path, e.what());
        return 1;
    }

    std::cout << std::format("Wrote {} particles to {} ({})\n",
                             data.particle_count(),
                             output_path,
                             format_name(output_format));

    return 0;
}
