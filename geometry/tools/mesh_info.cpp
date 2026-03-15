#include <iostream>
#include <CLI/CLI.hpp>
#include <fstream>
#include <optional>
#include <zipper/utils/minCoeff.hpp>
#include <zipper/utils/maxCoeff.hpp>
#include <zipper/utils/meanCoeff.hpp>
#include <nlohmann/json.hpp>
#include <balsa/geometry/triangle_mesh/read_obj.hpp>
#include <balsa/geometry/simplicial_complex/boundaries.hpp>
#include <balsa/geometry/simplicial_complex/volumes.hpp>
#include <balsa/geometry/bounding_box.hpp>

int main(int argc, char *argv[]) {
    CLI::App app{ "a simple mesh statistics tool", "mesh_info" };

    std::vector<std::string> filenames;
    bool do_all = false;
    bool use_json = false;
    std::string output_path;
    bool show_counts = false;
    bool show_bounding_box = false;
    bool show_volumes = false;

    app.add_option("filenames", filenames, "Input mesh files")
      ->required()
      ->check(CLI::ExistingFile);
    app.add_flag("-a,--all", do_all, "show all statistics");
    app.add_flag("-j,--json", use_json, "output as json");
    app.add_option("-o,--output", output_path, "output filepath");
    app.add_flag("-c,--counts", show_counts, "show element counts");
    app.add_flag("-b,--bounding_box", show_bounding_box, "show bounding box");
    app.add_flag("-v,--volumes", show_volumes, "show simplex volumes");

    CLI11_PARSE(app, argc, argv);

    using json = nlohmann::json;

    std::optional<std::ofstream> ofs_opt;
    if (!output_path.empty()) {
        ofs_opt.emplace(output_path.c_str());
    }

    std::ostream &os = ofs_opt ? *ofs_opt : std::cout;

    for (auto &&filename : filenames) {
        json js;
        if (use_json) {
            js["filename"] = filename;
        }

        auto mesh = balsa::geometry::triangle_mesh::read_objD(filename).position;

        auto [V, F, E] = mesh;
        if (do_all || show_counts) {
            if (E.cols() == 0) {
                E = balsa::geometry::simplicial_complex::boundaries(F);
            }
            if (use_json) {

                js["counts"] = { V.cols(), E.cols(), F.cols() };

            } else {
                os << "#V,#E,#F: " << V.cols() << "," << E.cols() << "," << F.cols() << std::endl;
            }
        }
        if (do_all || show_volumes) {
            auto EVol = balsa::geometry::simplicial_complex::volumes(V, E);
            auto FVol = balsa::geometry::simplicial_complex::volumes(V, F);
            auto get_stats = [](const auto &vec) {
                nlohmann::json js;
                double min = zipper::utils::minCoeff(vec);
                double max = zipper::utils::maxCoeff(vec);

                js["min"] = min;
                js["max"] = max;
                js["mean"] = vec.as_array().sum() / vec.extent(0);
                js["range"] = max - min;
                return js;
            };
            if (use_json) {
                js["volumes"]["edges"] = get_stats(EVol);
                js["volumes"]["triangles"] = get_stats(FVol);
            }
        }


        if (do_all || show_bounding_box) {
            auto bb = balsa::geometry::bounding_box(V);
            if (use_json) {

                const auto &m = bb.min();
                const auto &M = bb.max();
                // clang-format off
            js["bounding_box"] = {
                {"min",
                        {  m(0) ,
                          m(1) ,
                          m(2) }
                },
                {"max",
                        {  M(0) ,
                          M(1) ,
                          M(2) }
                }
            };
                // clang-format on
            } else {
                os << fmt::format("Bounding box: {} => {}", bb.min(), bb.max()) << std::endl;
            }
        }
        if (use_json) {
            os << js.dump(2) << std::endl;
        }
    }
}
