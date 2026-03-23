#include <catch2/catch_all.hpp>
#include <balsa/geometry/point_cloud/extxyz.hpp>
#include <balsa/geometry/point_cloud/read_xyz.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace balsa::geometry::point_cloud;

const static std::filesystem::path test_dir = BALSA_TEST_ASSET_PATH;

// ── is_extxyz_comment ────────────────────────────────────────────────────

TEST_CASE("is_extxyz_comment detects Properties=", "[extxyz][comment]") {
    CHECK(is_extxyz_comment("Properties=species:S:1:pos:R:3"));
    CHECK(is_extxyz_comment("Lattice=\"1 0 0 0 1 0 0 0 1\" Properties=species:S:1:pos:R:3"));
    CHECK_FALSE(is_extxyz_comment("This is a plain comment"));
    CHECK_FALSE(is_extxyz_comment(""));
    CHECK_FALSE(is_extxyz_comment("properties=species:S:1:pos:R:3"));// lowercase
}

// ── parse_extxyz_comment ─────────────────────────────────────────────────

TEST_CASE("parse simple Properties string", "[extxyz][comment]") {
    auto result = parse_extxyz_comment("Properties=species:S:1:pos:R:3");

    // Properties should NOT be in the info map (it's extracted).
    CHECK(result.info.find("Properties") == result.info.end());

    REQUIRE(result.property_defs.size() == 2);
    CHECK(result.property_defs[0].name == "species");
    CHECK(result.property_defs[0].type == ExtXYZType::String);
    CHECK(result.property_defs[0].cols == 1);
    CHECK(result.property_defs[1].name == "pos");
    CHECK(result.property_defs[1].type == ExtXYZType::Real);
    CHECK(result.property_defs[1].cols == 3);
}

TEST_CASE("parse comment with multiple key-value pairs", "[extxyz][comment]") {
    auto result = parse_extxyz_comment(
      R"(Lattice="5.44 0 0 0 5.44 0 0 0 5.44" Properties=species:S:1:pos:R:3 pbc="T T T" energy=-34.56)");

    // Lattice value should be unquoted.
    REQUIRE(result.info.count("Lattice") == 1);
    CHECK(result.info["Lattice"] == "5.44 0 0 0 5.44 0 0 0 5.44");

    REQUIRE(result.info.count("pbc") == 1);
    CHECK(result.info["pbc"] == "T T T");

    REQUIRE(result.info.count("energy") == 1);
    CHECK(result.info["energy"] == "-34.56");

    REQUIRE(result.property_defs.size() == 2);
}

TEST_CASE("parse comment with all property types", "[extxyz][comment]") {
    auto result = parse_extxyz_comment(
      "Properties=species:S:1:pos:R:3:charge:I:1:active:L:1");

    REQUIRE(result.property_defs.size() == 4);
    CHECK(result.property_defs[0].type == ExtXYZType::String);
    CHECK(result.property_defs[1].type == ExtXYZType::Real);
    CHECK(result.property_defs[2].type == ExtXYZType::Integer);
    CHECK(result.property_defs[3].type == ExtXYZType::Logical);
}

TEST_CASE("parse comment with no Properties defaults to empty defs", "[extxyz][comment]") {
    auto result = parse_extxyz_comment("This is a plain comment with no special keys");
    CHECK(result.property_defs.empty());
}

// ── build_properties_string ──────────────────────────────────────────────

TEST_CASE("build_properties_string round-trips", "[extxyz][comment]") {
    std::vector<ExtXYZPropertyDef> defs = {
        { "species", ExtXYZType::String, 1 },
        { "pos", ExtXYZType::Real, 3 },
        { "forces", ExtXYZType::Real, 3 },
    };
    auto s = build_properties_string(defs);
    CHECK(s == "species:S:1:pos:R:3:forces:R:3");

    // Parse it back.
    auto parsed = parse_extxyz_comment("Properties=" + s);
    REQUIRE(parsed.property_defs.size() == 3);
    CHECK(parsed.property_defs[0].name == "species");
    CHECK(parsed.property_defs[2].name == "forces");
    CHECK(parsed.property_defs[2].cols == 3);
}

// ── build_extxyz_comment ─────────────────────────────────────────────────

TEST_CASE("build_extxyz_comment includes Lattice and pbc first", "[extxyz][comment]") {
    std::map<std::string, std::string> info;
    info["energy"] = "-34.56";
    info["Lattice"] = "5.44 0 0 0 5.44 0 0 0 5.44";
    info["pbc"] = "T T T";

    std::vector<ExtXYZPropertyDef> defs = {
        { "species", ExtXYZType::String, 1 },
        { "pos", ExtXYZType::Real, 3 },
    };
    auto line = build_extxyz_comment(info, defs);

    // Lattice should come first, then Properties, then pbc, then rest.
    auto lattice_pos = line.find("Lattice=");
    auto props_pos = line.find("Properties=");
    auto pbc_pos = line.find("pbc=");
    auto energy_pos = line.find("energy=");

    CHECK(lattice_pos < props_pos);
    CHECK(props_pos < pbc_pos);
    CHECK(pbc_pos < energy_pos);

    // Lattice value should be quoted (contains spaces).
    CHECK(line.find("Lattice=\"5.44 0 0 0 5.44 0 0 0 5.44\"") != std::string::npos);
}

// ── Read silicon_bulk.extxyz ─────────────────────────────────────────────

TEST_CASE("read silicon bulk extxyz", "[extxyz][read]") {
    auto path = test_dir / "silicon_bulk.extxyz";
    REQUIRE(std::filesystem::exists(path));

    auto frame = read_extxyz_frame(path.string());

    CHECK(frame.atom_count == 8);

    // Check properties exist.
    auto names = frame.property_names();
    REQUIRE(names.size() == 3);
    CHECK(names[0] == "species");
    CHECK(names[1] == "pos");
    CHECK(names[2] == "forces");

    // Check species.
    auto sp = frame.species();
    REQUIRE(sp.size() == 8);
    for (const auto &s : sp) {
        CHECK(s == "Si");
    }

    // Check positions.
    auto pos = frame.positions();
    CHECK(pos.cols() == 8);
    // First atom: 0, 0, 0
    CHECK(pos(0, 0) == Catch::Approx(0.0));
    CHECK(pos(1, 0) == Catch::Approx(0.0));
    CHECK(pos(2, 0) == Catch::Approx(0.0));
    // Second atom: 1.36, 1.36, 1.36
    CHECK(pos(0, 1) == Catch::Approx(1.36));
    CHECK(pos(1, 1) == Catch::Approx(1.36));
    CHECK(pos(2, 1) == Catch::Approx(1.36));

    // Check forces property.
    auto *forces = frame.property("forces");
    REQUIRE(forces != nullptr);
    CHECK(forces->type == ExtXYZType::Real);
    CHECK(forces->cols == 3);
    REQUIRE(forces->real_data.has_value());
    // First atom forces: 0.1, -0.2, 0.3
    CHECK((*forces->real_data)(0, 0) == Catch::Approx(0.1));
    CHECK((*forces->real_data)(1, 0) == Catch::Approx(-0.2));
    CHECK((*forces->real_data)(2, 0) == Catch::Approx(0.3));

    // Check lattice.
    REQUIRE(frame.lattice.has_value());
    auto &lat = *frame.lattice;
    CHECK(lat(0, 0) == Catch::Approx(5.44));
    CHECK(lat(1, 1) == Catch::Approx(5.44));
    CHECK(lat(2, 2) == Catch::Approx(5.44));
    CHECK(lat(0, 1) == Catch::Approx(0.0));

    // Check pbc.
    REQUIRE(frame.pbc.has_value());
    CHECK((*frame.pbc)[0] == true);
    CHECK((*frame.pbc)[1] == true);
    CHECK((*frame.pbc)[2] == true);

    // Check info keys.
    CHECK(frame.info.count("energy") == 1);
    CHECK(frame.info["energy"] == "-34.56");
}

// ── Read multi-frame ─────────────────────────────────────────────────────

TEST_CASE("read multi-frame extxyz", "[extxyz][read]") {
    auto path = test_dir / "multi_frame.extxyz";
    REQUIRE(std::filesystem::exists(path));

    auto frames = read_extxyz(path.string());
    REQUIRE(frames.size() == 2);

    // Frame 0.
    CHECK(frames[0].atom_count == 3);
    CHECK(frames[0].info["frame"] == "0");
    auto pos0 = frames[0].positions();
    CHECK(pos0(0, 0) == Catch::Approx(0.0));// H at origin
    CHECK(pos0(0, 1) == Catch::Approx(1.0));// H at x=1

    // Frame 1 — shifted by 0.1 in x.
    CHECK(frames[1].atom_count == 3);
    CHECK(frames[1].info["frame"] == "1");
    auto pos1 = frames[1].positions();
    CHECK(pos1(0, 0) == Catch::Approx(0.1));
    CHECK(pos1(0, 1) == Catch::Approx(1.1));
}

// ── Read all property types ──────────────────────────────────────────────

TEST_CASE("read all property types (S, R, I, L)", "[extxyz][read]") {
    auto path = test_dir / "all_types.extxyz";
    REQUIRE(std::filesystem::exists(path));

    auto frame = read_extxyz_frame(path.string());
    CHECK(frame.atom_count == 2);

    // Species (S).
    auto sp = frame.species();
    REQUIRE(sp.size() == 2);
    CHECK(sp[0] == "Si");
    CHECK(sp[1] == "O");

    // Positions (R:3).
    auto pos = frame.positions();
    CHECK(pos(0, 0) == Catch::Approx(1.0));
    CHECK(pos(1, 0) == Catch::Approx(2.0));
    CHECK(pos(2, 0) == Catch::Approx(3.0));

    // charge (I:1).
    auto *charge = frame.property("charge");
    REQUIRE(charge != nullptr);
    CHECK(charge->type == ExtXYZType::Integer);
    CHECK(charge->cols == 1);
    REQUIRE(charge->int_data.has_value());
    CHECK((*charge->int_data)(0, 0) == 4);
    CHECK((*charge->int_data)(0, 1) == -2);

    // active (L:1).
    auto *active = frame.property("active");
    REQUIRE(active != nullptr);
    CHECK(active->type == ExtXYZType::Logical);
    CHECK(active->cols == 1);
    REQUIRE(active->bool_data.has_value());
    CHECK((*active->bool_data)[0] == true);
    CHECK((*active->bool_data)[1] == false);

    // Info key.
    CHECK(frame.info["info_key"] == "some value");
}

// ── Read with default Properties ─────────────────────────────────────────

TEST_CASE("read extxyz with no Properties defaults to species:S:1:pos:R:3", "[extxyz][read]") {
    auto path = test_dir / "default_props.extxyz";
    REQUIRE(std::filesystem::exists(path));

    auto frame = read_extxyz_frame(path.string());
    CHECK(frame.atom_count == 2);

    auto names = frame.property_names();
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "species");
    CHECK(names[1] == "pos");

    auto sp = frame.species();
    REQUIRE(sp.size() == 2);
    CHECK(sp[0] == "Si");
    CHECK(sp[1] == "O");

    auto pos = frame.positions();
    CHECK(pos(0, 0) == Catch::Approx(1.0));
    CHECK(pos(1, 1) == Catch::Approx(5.0));
}

// ── Write and round-trip ─────────────────────────────────────────────────

TEST_CASE("write and re-read single frame", "[extxyz][write]") {
    // Build a frame programmatically.
    ExtXYZFrame frame;
    frame.atom_count = 3;

    // Species.
    ExtXYZProperty species_prop;
    species_prop.type = ExtXYZType::String;
    species_prop.cols = 1;
    species_prop.string_data = std::vector<std::string>{ "H", "H", "O" };
    frame.properties.emplace_back("species", std::move(species_prop));

    // Positions.
    ExtXYZProperty pos_prop;
    pos_prop.type = ExtXYZType::Real;
    pos_prop.cols = 3;
    balsa::ColVectors<double, std::dynamic_extent> pos_data(3, 3);
    pos_data(0, 0) = 0.0;
    pos_data(1, 0) = 0.0;
    pos_data(2, 0) = 0.0;
    pos_data(0, 1) = 1.0;
    pos_data(1, 1) = 0.0;
    pos_data(2, 1) = 0.0;
    pos_data(0, 2) = 0.5;
    pos_data(1, 2) = 0.866;
    pos_data(2, 2) = 0.0;
    pos_prop.real_data = std::move(pos_data);
    frame.properties.emplace_back("pos", std::move(pos_prop));

    // Lattice + pbc.
    balsa::Mat3d lat;
    lat(0, 0) = 10.0;
    lat(0, 1) = 0.0;
    lat(0, 2) = 0.0;
    lat(1, 0) = 0.0;
    lat(1, 1) = 10.0;
    lat(1, 2) = 0.0;
    lat(2, 0) = 0.0;
    lat(2, 1) = 0.0;
    lat(2, 2) = 10.0;
    frame.lattice = lat;
    frame.pbc = std::array<bool, 3>{ true, true, false };

    // Info.
    frame.info["config_type"] = "water";

    // Write to a temp file.
    auto tmp = std::filesystem::temp_directory_path() / "test_extxyz_roundtrip.extxyz";
    write_extxyz(tmp.string(), frame);

    // Read back.
    auto frame2 = read_extxyz_frame(tmp.string());

    CHECK(frame2.atom_count == 3);
    auto sp = frame2.species();
    REQUIRE(sp.size() == 3);
    CHECK(sp[0] == "H");
    CHECK(sp[2] == "O");

    auto pos = frame2.positions();
    CHECK(pos(0, 1) == Catch::Approx(1.0));
    CHECK(pos(1, 2) == Catch::Approx(0.866));

    REQUIRE(frame2.lattice.has_value());
    CHECK((*frame2.lattice)(0, 0) == Catch::Approx(10.0));
    CHECK((*frame2.lattice)(2, 2) == Catch::Approx(10.0));

    REQUIRE(frame2.pbc.has_value());
    CHECK((*frame2.pbc)[0] == true);
    CHECK((*frame2.pbc)[1] == true);
    CHECK((*frame2.pbc)[2] == false);

    CHECK(frame2.info["config_type"] == "water");

    std::filesystem::remove(tmp);
}

TEST_CASE("write and re-read multi-frame", "[extxyz][write]") {
    std::vector<ExtXYZFrame> frames(2);

    for (int f = 0; f < 2; ++f) {
        auto &frame = frames[f];
        frame.atom_count = 2;

        ExtXYZProperty species_prop;
        species_prop.type = ExtXYZType::String;
        species_prop.cols = 1;
        species_prop.string_data = std::vector<std::string>{ "Ar", "Ar" };
        frame.properties.emplace_back("species", std::move(species_prop));

        ExtXYZProperty pos_prop;
        pos_prop.type = ExtXYZType::Real;
        pos_prop.cols = 3;
        balsa::ColVectors<double, std::dynamic_extent> pos_data(3, 2);
        pos_data(0, 0) = 0.0 + f;
        pos_data(1, 0) = 0.0;
        pos_data(2, 0) = 0.0;
        pos_data(0, 1) = 3.0 + f;
        pos_data(1, 1) = 0.0;
        pos_data(2, 1) = 0.0;
        pos_prop.real_data = std::move(pos_data);
        frame.properties.emplace_back("pos", std::move(pos_prop));

        frame.info["frame"] = std::to_string(f);
    }

    auto tmp = std::filesystem::temp_directory_path() / "test_extxyz_multi_roundtrip.extxyz";
    write_extxyz(tmp.string(), frames);

    auto frames2 = read_extxyz(tmp.string());
    REQUIRE(frames2.size() == 2);

    for (int f = 0; f < 2; ++f) {
        CHECK(frames2[f].atom_count == 2);
        auto pos = frames2[f].positions();
        CHECK(pos(0, 0) == Catch::Approx(0.0 + f));
        CHECK(pos(0, 1) == Catch::Approx(3.0 + f));
        CHECK(frames2[f].info["frame"] == std::to_string(f));
    }

    std::filesystem::remove(tmp);
}

// ── read_xyz auto-detection ──────────────────────────────────────────────

TEST_CASE("read_xyz auto-detects extxyz and returns XYZData", "[extxyz][read_xyz]") {
    auto path = test_dir / "silicon_bulk.extxyz";
    REQUIRE(std::filesystem::exists(path));

    // read_xyz should detect Properties= in the comment line and delegate.
    auto data = read_xyzD(path.string());

    REQUIRE(data.positions.cols() == 8);
    // First atom at origin.
    CHECK(data.positions(0, 0) == Catch::Approx(0.0));
    CHECK(data.positions(1, 0) == Catch::Approx(0.0));
    CHECK(data.positions(2, 0) == Catch::Approx(0.0));

    // Species should be preserved.
    REQUIRE(data.species.size() == 8);
    CHECK(data.species[0] == "Si");
}

// ── Error cases ──────────────────────────────────────────────────────────

TEST_CASE("read_extxyz throws on nonexistent file", "[extxyz][error]") {
    CHECK_THROWS_AS(read_extxyz("/nonexistent/path.extxyz"), std::runtime_error);
}

TEST_CASE("parse_extxyz_comment throws on bad Properties format", "[extxyz][error]") {
    // Properties must have triplets of name:type:cols.
    CHECK_THROWS_AS(parse_extxyz_comment("Properties=species:S"), std::runtime_error);
}

TEST_CASE("parse_extxyz_comment throws on unknown type char", "[extxyz][error]") {
    CHECK_THROWS_AS(parse_extxyz_comment("Properties=species:X:1"), std::runtime_error);
}

TEST_CASE("frame without pos property throws on positions()", "[extxyz][error]") {
    ExtXYZFrame frame;
    frame.atom_count = 0;
    CHECK_THROWS_AS(frame.positions(), std::runtime_error);
}
