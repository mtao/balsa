#include "balsa/geometry/point_cloud/read_xyz.hpp"
#include "balsa/geometry/point_cloud/extxyz.hpp"
#include <charconv>
#include <fstream>
#include <ranges>
#include <algorithm>
#include <iterator>
#include <array>
#include <string>

namespace balsa::geometry::point_cloud {

namespace {
    // Parse a single XYZ data line: "<species> x y z [vx vy vz]"
    // Returns the species string and up to 6 numeric values.
    struct ParsedLine {
        std::string species;
        std::array<double, 6> values;
    };

    ParsedLine parse_xyz_line(const std::string &line) {
        ParsedLine result;
        result.values.fill(0.0);

        auto toks = line | std::views::split(' ') | std::views::filter([](const auto &rng) {
                        return !rng.empty();
                    });

        auto it = toks.begin();
        if (it == toks.end()) {
            return result;
        }

        // First token is the species
        result.species = *it | std::ranges::to<std::string>();
        ++it;

        // Remaining tokens are numeric values
        int idx = 0;
        for (; it != toks.end() && idx < 6; ++it, ++idx) {
            auto s = *it | std::ranges::to<std::string>();
            std::from_chars(s.data(), s.data() + s.size(), result.values[idx]);
        }

        return result;
    }
}// namespace

XYZData read_xyzD(const std::string &filename) {
    // Peek at the comment line to check for Extended XYZ format.
    {
        std::ifstream ifs(filename);
        if (!ifs) {
            throw std::runtime_error("Failed to open XYZ file: " + filename);
        }
        std::string line;
        std::getline(ifs, line);// atom count
        std::getline(ifs, line);// comment
        if (is_extxyz_comment(line)) {
            // Delegate to extxyz reader and convert.
            auto frame = read_extxyz_frame(filename);
            XYZData data;
            data.positions = frame.positions();
            data.species = frame.species();
            // Store the original comment line reconstruction.
            // Build it from info + property defs so round-tripping works.
            data.comment = line;

            // Map "extra" columns: look for velocities or forces or any
            // 3-column real property that isn't "pos" or "species".
            for (const auto &[name, prop] : frame.properties) {
                if (name == "pos" || name == "species") continue;
                if (prop.type == ExtXYZType::Real && prop.cols == 3 && prop.real_data) {
                    // Use the first 3-column real property as extra.
                    data.extra = balsa::ColVecs3d(3, frame.atom_count);
                    for (size_t i = 0; i < frame.atom_count; ++i) {
                        for (int r = 0; r < 3; ++r) {
                            data.extra(r, i) = (*prop.real_data)(r, i);
                        }
                    }
                    break;
                }
            }
            return data;
        }
    }

    // Standard XYZ format.
    std::ifstream ifs(filename);
    if (!ifs) {
        throw std::runtime_error("Failed to open XYZ file: " + filename);
    }

    XYZData data;

    // Line 1: atom count
    std::string line;
    std::getline(ifs, line);
    int count = std::stoi(line);

    // Line 2: comment
    std::getline(ifs, line);
    data.comment = line;

    // Parse atom lines
    data.species.reserve(count);
    balsa::ColVectors<double, 6> D(6, count);

    int col = 0;
    while (std::getline(ifs, line) && col < count) {
        auto parsed = parse_xyz_line(line);
        data.species.push_back(std::move(parsed.species));
        for (int r = 0; r < 6; ++r) {
            D(r, col) = parsed.values[r];
        }
        ++col;
    }

    data.positions = D.template topRows<3>();

    if (D.bottomRows(3).as_array().abs().sum() != 0) {
        data.extra = D.template bottomRows<3>();
    }

    return data;
}

std::array<balsa::ColVectors<float, 3>, 2> read_xyzF(const std::string &filename) {
    auto data = read_xyzD(filename);
    std::array<balsa::ColVectors<float, 3>, 2> result;
    result[0] = data.positions.template cast<float>();
    if (data.extra.cols() > 0) {
        result[1] = data.extra.template cast<float>();
    }
    return result;
}

}// namespace balsa::geometry::point_cloud
