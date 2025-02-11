#include <iostream>
#include <cxxopts.hpp>
#include <fstream>
#include <optional>
#include <nlohmann/json.hpp>
#include <balsa/geometry/triangle_mesh/read_obj.hpp>
#include <balsa/geometry/simplicial_complex/boundaries.hpp>
#include <balsa/geometry/simplicial_complex/volumes.hpp>
#include <balsa/geometry/bounding_box.hpp>

int main(int argc, char *argv[]) {
    cxxopts::Options opts("mesh_view", "a simple mesh statistics tool");

    // clang-format off
    opts.add_options()
        ("filenames", "Arguments without options",cxxopts::value<std::vector<std::string>>())
        ("a,all", "show all statistics", cxxopts::value<bool>()->default_value("false"))
        ("j,json", "output as json", cxxopts::value<bool>()->default_value("false"))
        ("o,output", "output filepath", cxxopts::value<std::string>())
        ("c,counts", "show element counts", cxxopts::value<bool>()->default_value("false"))
        ("b,bounding_box", "show bounding box", cxxopts::value<bool>()->default_value("false"))
        ("v,volumes", "show simplex volumes", cxxopts::value<bool>()->default_value("false"))
        ("h,help", "Print usage");

    opts.parse_positional({"filenames"});

    // clang-format on

    using json = nlohmann::json;
    auto res = opts.parse(argc, argv);

    if (res.count("help")) {
        std::cout << opts.help() << std::endl;
        return 0;
    }

    std::optional<std::ofstream> ofs_opt;
    if (res.count("output")) {
        ofs_opt.emplace(res["output"].as<std::string>().c_str());
    }

    std::ostream &os = ofs_opt ? *ofs_opt : std::cout;

    bool use_json = res["json"].as<bool>();
    bool do_all = res["all"].as<bool>();

#if 0
    std::vector<std::string> filenames;
    filenames.emplace_back(
      res["filenames"].as<std::string>());
#else
    auto filenames = res["filenames"].as<std::vector<std::string>>();
#endif
    for (auto &&filename : filenames) {
        json js;
        if (use_json) {
            js["filename"] = filename;
        }

        auto mesh = balsa::geometry::triangle_mesh::read_objD(filename).position;

        auto [V, F, E] = mesh;
        if (do_all || res["simplex_counts"].as<bool>()) {
            if (E.cols() == 0) {
                E = balsa::geometry::simplicial_complex::boundaries(F);
            }
            if (use_json) {

                js["counts"] = { V.cols(), E.cols(), F.cols() };

            } else {
                os << "#V,#E,#F: " << V.cols() << "," << E.cols() << "," << F.cols() << std::endl;
            }
        }
        if (do_all || res["volumes"].as<bool>()) {
            auto EVol = balsa::geometry::simplicial_complex::volumes(V, E);
            auto FVol = balsa::geometry::simplicial_complex::volumes(V, F);
            auto get_stats = [](const auto &vec) {
                nlohmann::json js;
                double min = vec.minCoeff();
                double max = vec.maxCoeff();

                js["min"] = min;
                js["max"] = max;
                js["mean"] = vec.sum() / vec.size();
                js["range"] = max - min;
                return js;
            };
            if (use_json) {
                js["volumes"]["edges"] = get_stats(EVol);
                js["volumes"]["triangles"] = get_stats(FVol);
            }
        }


        if (do_all || res["bounding_box"].as<bool>()) {
            auto bb = balsa::geometry::bounding_box(V);
            if (use_json) {

                const auto &m = bb.min();
                const auto &M = bb.max();
                // clang-format off
            js["bounding_box"] = {
                {"min",
                        {  m.x() ,
                          m.y() ,
                          m.z() }
                },
                {"max",
                        {  M.x() ,
                          M.y() ,
                          M.z() }
                }
            };
                // clang-format on
            } else {
                os << "Bounding box: " << bb.min().transpose() << " => " << bb.max().transpose() << std::endl;
            }
        }
        if (use_json) {
            os << js.dump(2) << std::endl;
        }
    }
}
