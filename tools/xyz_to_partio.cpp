
#include <balsa/geometry/point_cloud/read_xyz.hpp>
#include <balsa/geometry/point_cloud/partio_loader.hpp>
#include <balsa/eigen/stack.hpp>
#include <iostream>


int main(int argc, char *argv[]) {
    auto [V, N] = balsa::geometry::point_cloud::read_xyzD(argv[1]);


    bool use_color = N.size() > 0;
    int size = V.cols();


    balsa::geometry::point_cloud::PartioFileWriter w(argv[2]);
    w.set_positions(V);
    if (use_color) {
        w.set_colors(balsa::eigen::vstack(N, balsa::eigen::RowVecXd::Ones(size)));
    }

    if (argc > 3) {
        double scale = std::stof(argv[3]);
        w.set_radii(balsa::eigen::VecXd::Constant(size, scale));
    }


    return 0;
}
