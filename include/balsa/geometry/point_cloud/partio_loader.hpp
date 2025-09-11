#if !defined(BALSA_GEOMETRY_POINT_CLOUD_PARTIO_LOADER_HPP)
#define BALSA_GEOMETRY_POINT_CLOUD_PARTIO_LOADER_HPP
#include "balsa/tensor_types.hpp"

#include "balsa/zipper/concepts/shape_types.hpp"

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
    void set_positions(const ColVecs3d &P);
    void set_velocities(const ColVecs3d &V);
    void set_colors(const ColVecs4d &C);
    void set_radii(const VecXd &R);
    void set_densities(const VecXd &R);
    void set_ids(const VecXi &I);
    void update_size(int size);
    void write();

    void set_attribute(const std::string &name, balsa::zipper::concepts::ColVecsDCompatible auto const &V);

    void set_attribute(const std::string &name, ::zipper::concepts::VectorBaseDerived auto const &V);

  private:
    std::string _filename;
    Partio::ParticlesDataMutable *_handle;
};

struct PartioFileReader {
  public:
    PartioFileReader(const std::string &filename);
    ~PartioFileReader();
    ColVecs3d positions() const;
    ColVecs3d velocities() const;
    ColVecs4d colors() const;
    VecXd radii() const;
    VecXd densities() const;
    VecXi ids() const;


    template<typename T, rank_type D>
    ColVectors<T, D> vector_attribute(const std::string &name) const;
    template<typename T>
    VectorX<T> attribute(const std::string &name) const;
    bool has_positions() const;
    bool has_velocities() const;
    bool has_colors() const;
    bool has_densities() const;
    bool has_radii() const;
    bool has_ids() const;
    std::vector<std::string> attributes() const;

    index_type particle_count() const;


    template<typename T, rank_type D = 1>
    bool has_attribute(const std::string &name) const;


  private:
    Partio::ParticlesData *_handle;
};


ColVecs3d points_from_partio(const std::string &filename);
std::tuple<ColVecs3d, ColVecs3d> points_and_velocity_from_partio(const std::string &filename);


void write_to_partio(const std::string &filename, const ColVecs3d &P);
void write_to_partio(const std::string &filename, const ColVecs3d &P, const ColVecs3d &V);
}// namespace balsa::geometry::point_cloud
#endif
