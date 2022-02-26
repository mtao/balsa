
#if !defined(BALSA_GEOMETRY_POLYGON_MESH_READ_OBJ_IMPL_H)
#define BALSA_GEOMETRY_POLYGON_MESH_READ_OBJ_IMPL_H
#include "read_obj.hpp"
#include "balsa/geometry/polygon_mesh/polygon_buffer_construction.hpp"
#include "balsa/eigen/stl2eigen.hpp"
#include <fstream>
#include <range/v3/view/take_exactly.hpp>
#include <optional>
#include <range/v3/view/split.hpp>
#include <range/v3/view/stride.hpp>
#include <range/v3/view/remove_if.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/zip.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/getlines.hpp>
#include <range/v3/algorithm/copy.hpp>
#include <charconv>

namespace balsa::geometry::polygon_mesh {

namespace {

    template<typename T>
    auto read_value(auto &&v) {
        T value;
        auto s = v | ranges::to<std::string>;
        std::from_chars(s.data(), s.data() + s.size(), value);
        return value;
    }
    template<typename T>
    auto read_values(auto &&values) {

        return values | ranges::views::transform([](const auto &v) {
                   return read_value<T>(v);
               });
    }

}// namespace

template<typename Scalar, int D>
OBJMesh<Scalar, D> read_obj(const std::string &filename) {
    std::ifstream ifs(filename);

    auto file_lines = ranges::getlines(ifs);

    std::vector<std::array<Scalar, D>> v;
    std::vector<std::vector<int>> vi;
    std::vector<std::array<int, 2>> vl;

    std::vector<std::array<Scalar, 2>> t;
    std::vector<std::vector<int>> ti;
    std::vector<std::array<int, 2>> tl;

    std::vector<std::array<Scalar, D>> n;
    std::vector<std::vector<int>> ni;
    std::vector<std::array<int, 2>> nl;

    for (auto &&line : file_lines) {

        auto toks = line | ranges::views::split(' ') | ranges::views::remove_if([](const auto &rng) {
                        return rng.empty();
                    });
        auto front = toks.front() | ranges::to<std::string>();
        auto data = toks | ranges::views::drop(1);
        //std::cout << front << ": " << data << std::endl;
        size_t size = front.size();
        switch (size) {
        case 1: {
            switch (front[0]) {
            case 'v':// v
            {
                auto &back = v.emplace_back();
                auto dat = read_values<Scalar>(data) | ranges::views::take_exactly(D);
                ranges::copy(dat, back.begin());
                break;
            }
            case 'f':// f

            {
                std::vector<int> v;
                std::vector<int> n;
                std::vector<int> t;
                std::array<std::vector<int> *, 3> ptrs{ &v, &t, &n };
                for (auto &&tok : data) {
                    auto slashtoks = tok | ranges::views::split('/');
                    auto zip = ranges::views::zip(ptrs, slashtoks);
                    for (auto &&[vecptr, str] : zip) {
                        auto s = str | ranges::to<std::string>;
                        if (!s.empty()) {
                            int value;
                            std::from_chars(s.data(), s.data() + s.size(), value);
                            vecptr->emplace_back(value);
                        }
                    }
                }
                //std::cout << slashed_toks << std::endl;
                //std::cout << (slashed_toks | ranges::views::stride(0)) << std::endl;
                //auto v = slashed_toks | ranges::views::stride(0) | ranges::views::transform([](const std::optional<int> &i) { return *i; }) | ranges::to<std::vector<int>>;
                //std::cout << v.size() << std::endl;
                //auto n = slashed_toks | ranges::views::stride(1) | ranges::to<std::vector<int>>;
                //auto t = slashed_toks | ranges::views::stride(2) | ranges::to<std::vector<int>>;
                if (v.size() > 0) {
                    vi.emplace_back(v);
                }
                if (t.size() > 0) {
                    ti.emplace_back(t);
                }
                if (n.size() > 0) {
                    ni.emplace_back(n);
                }
                break;
            }
            case 'l':// l
            {
                //ranges::copy(read_values<int>(data) | ranges::views::take_exactly(2), back.begin());
                break;
            }
            default:
                break;
            }

            break;
        }
        case 2: {
            if (front[0] == 'v') {
                switch (front[1]) {
                case 't':// vt
                {
                    auto &back = v.emplace_back();
                    auto dat = read_values<Scalar>(data) | ranges::views::take_exactly(2);
                    ranges::copy(dat, back.begin());
                    break;
                }
                case 'n':// vn
                {
                    auto &back = n.emplace_back();
                    auto dat = read_values<Scalar>(data) | ranges::views::take_exactly(D);
                    ranges::copy(dat, back.begin());
                    break;
                }
                default:
                    break;
                }
            }
            break;
        }
        default:
            break;
        }
    }

    return OBJMesh<Scalar, D>{
        .position = PolygonMesh<Scalar, D>{
          eigen::ColVectors<Scalar, D>(balsa::eigen::stl2eigen(v)), from_constainer_container<int>(vi), {} },
        .texture = PolygonMesh<Scalar, 2>{ eigen::ColVectors<Scalar, 2>(balsa::eigen::stl2eigen(t)), from_constainer_container<int>(ti), {} },
        .normal = PolygonMesh<Scalar, D>{ eigen::ColVectors<Scalar, D>(balsa::eigen::stl2eigen(n)), from_constainer_container<int>(ni), {} },
    };
}
}// namespace balsa::geometry::polygon_mesh

#endif
