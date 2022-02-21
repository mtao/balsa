#include "balsa/geometry/point_cloud/partio_loader_impl.hpp"
namespace balsa::geometry::point_cloud

{

PartioFileWriter::PartioFileWriter(const std::string &filename) : _filename(filename), _handle(Partio::create()) {
    if (!_handle) throw std::invalid_argument(fmt::format("failed to open {}", filename));
}
PartioFileWriter::~PartioFileWriter() {
    write();
    _handle->release();
}

void PartioFileWriter::update_size(int size) {

    int cur = _handle->numParticles();
    if (cur < size) {
        _handle->addParticles(size - cur);
    }
}
void PartioFileWriter::set_positions(const balsa::eigen::ColVecs3d &P) {

    set_attribute("position", P.cast<float>());
}
void PartioFileWriter::set_velocities(const balsa::eigen::ColVecs3d &V) {
    set_attribute("velocity", V.cast<float>());
}
void PartioFileWriter::set_colors(const balsa::eigen::ColVecs4d &C) {
    set_attribute("color", C.cast<float>());
}
void PartioFileWriter::set_ids(const balsa::eigen::VecXi &I) {
    set_attribute("id", I);
}
void PartioFileWriter::set_radii(const balsa::eigen::VecXd &I) {
    set_attribute("radius", I);
}
void PartioFileWriter::set_densities(const balsa::eigen::VecXd &I) {
    set_attribute("density", I);
}

void PartioFileWriter::write() {

    Partio::write(_filename.c_str(), *_handle);
}

PartioFileReader::PartioFileReader(const std::string &filename) : _handle(Partio::read(filename.c_str())) {
    if (!_handle) throw std::invalid_argument(fmt::format("failed to open {}", filename));
}
PartioFileReader::~PartioFileReader() {
    _handle->release();
}
int PartioFileReader::particle_count() const {
    return _handle->numParticles();
}


std::vector<std::string> PartioFileReader::attributes() const {
    int num_attrs = _handle->numAttributes();
    std::vector<std::string> attr_names(num_attrs);
    for (int j = 0; j < num_attrs; ++j) {
        Partio::ParticleAttribute attr;
        _handle->attributeInfo(j, attr);
        attr_names[j] = attr.name;
    }
    return attr_names;
}


balsa::eigen::ColVecs3d PartioFileReader::positions() const {
    return vector_attribute<float, 3>("position").cast<double>();
}
balsa::eigen::ColVecs3d PartioFileReader::velocities() const {
    return vector_attribute<float, 3>("velocity").cast<double>();
}
balsa::eigen::ColVecs4d PartioFileReader::colors() const {
    return vector_attribute<float, 4>("color").cast<double>();
}
balsa::eigen::VecXi PartioFileReader::ids() const {
    return attribute<int>("id");
}
balsa::eigen::VecXd PartioFileReader::densities() const {
    return attribute<float>("density").cast<double>();
}

balsa::eigen::VecXd PartioFileReader::radii() const {
    return attribute<float>("radius").cast<double>();
}


bool PartioFileReader::has_positions() const {
    return has_attribute<float, 3>("position");
}
bool PartioFileReader::has_velocities() const {
    return has_attribute<float, 3>("velocity");
}
bool PartioFileReader::has_colors() const {
    return has_attribute<float, 3>("color");
}
bool PartioFileReader::has_ids() const {
    return has_attribute<int>("id");
}

bool PartioFileReader::has_densities() const {
    return has_attribute<float>("density");
}

bool PartioFileReader::has_radii() const {
    return has_attribute<float, 1>("radius");
}


balsa::eigen::ColVecs3d points_from_partio(const std::string &filename) {
    return PartioFileReader(filename).positions();
}
std::tuple<balsa::eigen::ColVecs3d, balsa::eigen::ColVecs3d> points_and_velocity_from_partio(const std::string &filename) {
    PartioFileReader r(filename);
    return { r.positions(), r.velocities() };
}

void write_to_partio(const std::string &filename, const balsa::eigen::ColVecs3d &P) {
    PartioFileWriter w(filename);
    w.set_positions(P);
}
void write_to_partio(const std::string &filename, const balsa::eigen::ColVecs3d &P, const balsa::eigen::ColVecs3d &V) {
    PartioFileWriter writer(filename);
    writer.set_positions(P);
    writer.set_velocities(V);
}
}// namespace balsa::geometry::point_cloud
