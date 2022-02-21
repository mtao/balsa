#if !defined(BALSA_GEOMETRY_POINT_CLOUD_PARTIO_LOADER_HPP)
#define BALSA_GEOMETRY_POINT_CLOUD_PARTIO_LOADER_HPP
#include "balsa/eigen/types.hpp"
#include "balsa/eigen/shape_checks.hpp"


// NOTE: if you wish to use custom attributes you want to include partio_loader_impl.hpp
// this design is so that we can avoid designing
namespace Partio {
class ParticlesData;
class ParticlesDataMutable;
}// namespace Partio
namespace balsa::geometry::point_cloud {

// my abstraction of partio files. currently does not take advantage of the ID field at all
struct PartioFileWriter {
  public:
    PartioFileWriter(const std::string &filename);
    ~PartioFileWriter();
    void set_positions(const eigen::ColVecs3d &P);
    void set_velocities(const eigen::ColVecs3d &V);
    void set_colors(const eigen::ColVecs4d &C);
    void set_radii(const eigen::VecXd &R);
    void set_densities(const eigen::VecXd &R);
    void set_ids(const eigen::VecXi &I);
    void update_size(int size);
    void write();

    template<eigen::concepts::ColVecsDCompatible T>
    void set_attribute(const std::string &name, const T &V);

    template<eigen::concepts::VecXCompatible T>
    void set_attribute(const std::string &name, const T &V);

  private:
    std::string _filename;
    Partio::ParticlesDataMutable *_handle;
};

struct PartioFileReader {
  public:
    PartioFileReader(const std::string &filename);
    ~PartioFileReader();
    eigen::ColVecs3d positions() const;
    eigen::ColVecs3d velocities() const;
    eigen::ColVecs4d colors() const;
    eigen::VecXd radii() const;
    eigen::VecXd densities() const;
    eigen::VecXi ids() const;


    template<typename T, int D>
    eigen::ColVectors<T, D> vector_attribute(const std::string &name) const;
    template<typename T>
    eigen::VectorX<T> attribute(const std::string &name) const;
    bool has_positions() const;
    bool has_velocities() const;
    bool has_colors() const;
    bool has_densities() const;
    bool has_radii() const;
    bool has_ids() const;
    std::vector<std::string> attributes() const;

    int particle_count() const;


    template<typename T, int D = 1>
    bool has_attribute(const std::string &name) const;


  private:
    Partio::ParticlesData *_handle;
};


eigen::ColVecs3d points_from_partio(const std::string &filename);
std::tuple<eigen::ColVecs3d, eigen::ColVecs3d> points_and_velocity_from_partio(const std::string &filename);


void write_to_partio(const std::string &filename, const eigen::ColVecs3d &P);
void write_to_partio(const std::string &filename, const eigen::ColVecs3d &P, const eigen::ColVecs3d &V);
}// namespace geometry::point_cloud
#endif
