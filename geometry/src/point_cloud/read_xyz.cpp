#include "balsa/geometry/point_cloud/read_xyz.hpp"
#include "balsa/eigen/stl2eigen.hpp"
#include <range/v3/view/getlines.hpp>
#include <range/v3/view/remove_if.hpp>
#include <range/v3/algorithm/copy.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/drop.hpp>
#include <charconv>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/repeat.hpp>
#include <range/v3/view/take_exactly.hpp>
#include <range/v3/range/conversion.hpp>
#include <fstream>
#include <iterator>
#include <array>

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


    auto file_lines = ranges::getlines(ifs);

    auto data_lines =
      file_lines | ranges::views::transform([](const auto &line) {
          auto toks = line | ranges::views::split(' ') | ranges::views::remove_if([](const auto &rng) {
                          return rng.empty();
                      });
          // TODO: we might want to use the individual species somewhere
          // auto front = toks.front();
          auto nums = toks | ranges::views::drop(1) | ranges::views::transform([](const auto &v) {
                          T value;
                          auto s = v | ranges::to<std::string>;
                          std::from_chars(s.data(), s.data() + s.size(), value);
                          return value;
                      });
          std::array<T, 6> ret;
          auto inp = ranges::views::concat(nums, ranges::views::repeat(T(0))) | ranges::views::take_exactly(6);
          ranges::copy(inp, ret.begin());
          return ret;
      });


    balsa::ColVectors<T, 6> D(6, count);

    for (auto &&[col, arr] : ranges::views::enumerate(data_lines) | ranges::views::take_exactly(count)) {
        // std::cout << ranges::views::all(arr) << std::endl;
        ::zipper::VectorBase r = arr;
        D.col(col) = r;
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
