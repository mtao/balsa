#include "balsa/geometry/point_cloud/read_xyz.hpp"
#include <charconv>
#include <fstream>
#include <ranges>
#include <algorithm>
#include <iterator>
#include <array>
#include <string>

namespace balsa::geometry::point_cloud {
template<typename T>
std::array<balsa::ColVectors<T, 3>, 2> read_xyz(const std::string &filename) {
    std::vector<std::array<T, 6>> lines;


    std::ifstream ifs(filename);


    std::string line;
    int count = 0;
    std::getline(ifs, line);
    count = std::stof(line);
    // comment line
    std::getline(ifs, line);
    lines.resize(count);


    auto file_lines_transform = [](const std::string &line) {
        auto toks = line | std::views::split(' ') | std::views::filter([](const auto &rng) {
                        return !rng.empty();
                    });
        // TODO: we might want to use the individual species somewhere
        // auto front = toks.front();
        auto nums = toks | std::views::drop(1) | std::views::transform([](const auto &v) {
                        T value;
                        auto s = v | std::ranges::to<std::string>();
                        std::from_chars(s.data(), s.data() + s.size(), value);
                        return value;
                    });
        std::array<T, 6> ret;
        auto inp = std::views::concat(nums, std::views::repeat(T(0))) | std::views::take(6);
        std::ranges::copy(inp, ret.begin());
        return ret;
    };


    balsa::ColVectors<T, 6> D(6, count);

    int col = 0;
    while (std::getline(ifs, line) && col < count) {
        auto arr = file_lines_transform(line);
        ::zipper::VectorBase r = arr;
        D.col(col) = r;
        ++col;
    }


    if (D.bottomRows(3).as_array().abs().sum() == 0) {
        balsa::ColVectors<T, 3> U = D.template topRows<3>();
        return { { U, {} } };
    } else {
        balsa::ColVectors<T, 3> U = D.template topRows<3>();
        balsa::ColVectors<T, 3> L = D.template bottomRows<3>();
        return { { U, L } };
    }
}

std::array<balsa::ColVectors<float, 3>, 2> read_xyzF(const std::string &filename) {
    return read_xyz<float>(filename);
}
std::array<balsa::ColVectors<double, 3>, 2> read_xyzD(const std::string &filename) {
    return read_xyz<double>(filename);
}
}// namespace balsa::geometry::point_cloud
