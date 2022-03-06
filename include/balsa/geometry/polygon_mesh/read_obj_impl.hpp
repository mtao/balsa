
#if !defined(BALSA_GEOMETRY_POLYGON_MESH_READ_OBJ_IMPL_H)
#define BALSA_GEOMETRY_POLYGON_MESH_READ_OBJ_IMPL_H
#include "read_obj.hpp"
#include "balsa/eigen/stl2eigen.hpp"
#include <balsa/data_structures/container_of_containers_to_stacked_contiguous_buffer.hpp>
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
#include <filesystem>
#include <iostream>

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
OBJMesh<Scalar, D> read_obj(const std::filesystem::path &filename) {
    std::ifstream ifs(filename);

    auto file_lines = ranges::getlines(ifs);

    std::vector<std::array<Scalar, D>> v;
    std::vector<std::vector<int>> vi;
    std::vector<std::vector<int>> vl;

    std::vector<std::array<Scalar, 2>> t;
    std::vector<std::vector<int>> ti;
    std::vector<std::vector<int>> tl;

    std::vector<std::array<Scalar, D>> n;
    std::vector<std::vector<int>> ni;
    std::vector<std::vector<int>> nl;


    auto process_slash_toks = [](auto &&data) {
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
                    vecptr->emplace_back(value - 1);
                }
            }
        }
        return std::make_tuple(v, n, t);
    };

    for (auto &&line : file_lines) {

        auto toks = line | ranges::views::split(' ') | ranges::views::remove_if([](const auto &rng) {
                        return rng.empty();
                    });
        auto front = toks.front() | ranges::to<std::string>();
        auto data = toks | ranges::views::drop(1);
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
                auto [v, n, t] = process_slash_toks(data);
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
                auto [v, n, t] = process_slash_toks(data);
                if (v.size() > 0) {
                    vl.emplace_back(v);
                }
                if (t.size() > 0) {
                    tl.emplace_back(t);
                }
                if (n.size() > 0) {
                    nl.emplace_back(n);
                }
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
                    auto &back = t.emplace_back();
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
          eigen::ColVectors<Scalar, D>(balsa::eigen::stl2eigen(v)),//
          PolygonBuffer<int>{ data_structures::container_of_containers_to_stacked_contiguous_buffer<int>(vi) },//
          PLCurveBuffer<int>{ data_structures::container_of_containers_to_stacked_contiguous_buffer<int>(vl) },//
        },//
        .texture = PolygonMesh<Scalar, 2>{
          eigen::ColVectors<Scalar, 2>(balsa::eigen::stl2eigen(t)),//
          PolygonBuffer<int>{ data_structures::container_of_containers_to_stacked_contiguous_buffer<int>(ti) },//
          PLCurveBuffer<int>{ data_structures::container_of_containers_to_stacked_contiguous_buffer<int>(tl) },//
        },
        .normal = PolygonMesh<Scalar, D>{
          eigen::ColVectors<Scalar, D>(balsa::eigen::stl2eigen(n)),//
          PolygonBuffer<int>{ data_structures::container_of_containers_to_stacked_contiguous_buffer<int>(ni) },//
          PLCurveBuffer<int>{ data_structures::container_of_containers_to_stacked_contiguous_buffer<int>(nl) },//
        },
    };
}
}// namespace balsa::geometry::polygon_mesh

#endif
