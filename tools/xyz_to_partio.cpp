
#include <balsa/geometry/point_cloud/read_xyz.hpp>
#include <balsa/geometry/point_cloud/partio_loader.hpp>
#include <zipper/views/nullary/ConstantView.hpp>
// #include <balsa/eigen/stack.hpp>
#include <iostream>


int main(int argc, char *argv[]) {
    auto [V, N] = balsa::geometry::point_cloud::read_xyzD(argv[1]);


    bool use_color = N.cols() > 0;
    int size = V.cols();


    balsa::geometry::point_cloud::PartioFileWriter w(argv[2]);
    w.set_positions(V);
    if (use_color) {
        balsa::ColVecs4d C(size);
        C.topRows<3>() = N;
        C.row(3) = ::zipper::views::nullary::ConstantView<double>(1.0);
        w.set_colors(C);
        // w.set_colors(balsa::vstack(N, balsa::RowVecXd::Ones(size)));
    }

    if (argc > 3) {
        double scale = std::stof(argv[3]);
        w.set_radii(::zipper::views::nullary::ConstantView<double, std::dynamic_extent>(scale, ::zipper::create_dextents(size)));
    }


    return 0;
}
