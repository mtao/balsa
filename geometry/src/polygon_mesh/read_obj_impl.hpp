
#if !defined(BALSA_GEOMETRY_POLYGON_MESH_READ_OBJ_IMPL_H)
#define BALSA_GEOMETRY_POLYGON_MESH_READ_OBJ_IMPL_H
#include "balsa/geometry/polygon_mesh/read_obj.hpp"
#include "balsa/zipper/types.hpp"
#include <zipper/MatrixBase.hpp>
#include <balsa/data_structures/container_of_containers_to_stacked_contiguous_buffer.hpp>
#include <fstream>
#include <optional>
#include <ranges>
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <string>

namespace balsa::geometry::polygon_mesh {

namespace {

    template<typename T>
    auto read_value(auto &&v) {
        T value;
        auto s = v | std::ranges::to<std::string>();
        std::from_chars(s.data(), s.data() + s.size(), value);
        return value;
    }
    template<typename T>
    auto read_values(auto &&values) {

        return values | std::views::transform([](const auto &v) {
                   return read_value<T>(v);
               });
    }

}// namespace

template<typename Scalar, int8_t D>
OBJMesh<Scalar, D> read_obj(const std::filesystem::path &filename) {
    std::ifstream ifs(filename);


    std::vector<std::array<Scalar, D>> v;
    std::vector<std::vector<index_type>> vi;
    std::vector<std::vector<index_type>> vl;

    std::vector<std::array<Scalar, 2>> t;
    std::vector<std::vector<index_type>> ti;
    std::vector<std::vector<index_type>> tl;

    std::vector<std::array<Scalar, D>> n;
    std::vector<std::vector<index_type>> ni;
    std::vector<std::vector<index_type>> nl;


    auto process_slash_toks = [](auto &&data) {
        std::vector<index_type> v;
        std::vector<index_type> n;
        std::vector<index_type> t;
        std::array<std::vector<index_type> *, 3> ptrs{ &v, &t, &n };
        for (auto &&tok : data) {
            auto slashtoks = tok | std::views::split('/');
            auto zip = std::views::zip(ptrs, slashtoks);
            for (auto &&[vecptr, str] : zip) {
                auto s = str | std::ranges::to<std::string>();
                if (!s.empty()) {
                    index_type value = 0;
                    std::from_chars(s.data(), s.data() + s.size(), value);
                    vecptr->emplace_back(value - 1);
                }
            }
        }
        return std::make_tuple(v, n, t);
    };

    std::string line;
    while (std::getline(ifs, line)) {

        auto toks = line | std::views::split(' ') | std::views::filter([](const auto &rng) {
                        return !rng.empty();
                    });
        if (std::ranges::empty(toks)) { continue; }
        auto front = toks.front() | std::ranges::to<std::string>();
        if (front[0] == '#') { continue; }
        auto data = toks | std::views::drop(1);
        size_t size = front.size();
        switch (size) {
        case 1: {
            switch (front[0]) {
            case 'v':// v
            {
                auto &back = v.emplace_back();
                auto dat = read_values<Scalar>(data) | std::views::take(D);
                std::ranges::copy(dat, back.begin());
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
                    auto dat = read_values<Scalar>(data) | std::views::take(2);
                    std::ranges::copy(dat, back.begin());
                    break;
                }
                case 'n':// vn
                {
                    auto &back = n.emplace_back();
                    auto dat = read_values<Scalar>(data) | std::views::take(D);
                    std::ranges::copy(dat, back.begin());
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


    PolygonMesh<Scalar, D> pos{
        ColVectors<Scalar, D>(::zipper::MatrixBase(v).transpose()),//
        PolygonBuffer<index_type>{ data_structures::container_of_containers_to_stacked_contiguous_buffer<index_type>(vi) },//
        PLCurveBuffer<index_type>{ data_structures::container_of_containers_to_stacked_contiguous_buffer<index_type>(vl) },//
    };//
    PolygonMesh<Scalar, 2> tex{
        ColVectors<Scalar, 2>(::zipper::MatrixBase(t).transpose()),//
        PolygonBuffer<index_type>{ data_structures::container_of_containers_to_stacked_contiguous_buffer<index_type>(ti) },//
        PLCurveBuffer<index_type>{ data_structures::container_of_containers_to_stacked_contiguous_buffer<index_type>(tl) },//
    };
    PolygonMesh<Scalar, D> nor{
        ColVectors<Scalar, D>(::zipper::MatrixBase(n).transpose()),//
        PolygonBuffer<index_type>{ data_structures::container_of_containers_to_stacked_contiguous_buffer<index_type>(ni) },//
        PLCurveBuffer<index_type>{ data_structures::container_of_containers_to_stacked_contiguous_buffer<index_type>(nl) },//
    };
    using RType = OBJMesh<Scalar, D>;
    return RType{
        .position = std::move(pos),
        .texture = std::move(tex),
        .normal = std::move(nor)

    };
}
}// namespace balsa::geometry::polygon_mesh

#endif
