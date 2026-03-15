#pragma once
#include "balsa/eigen/types.hpp"
#include <openvdb/Types.h>

namespace balsa::geometry::point_cloud {
// Adaptor structs for use with openvdb::tools::ParticlesToLevelSet.
// Each struct implements the interface expected by OpenVDB's particle tools.

struct VDBParticleList_Pos {
    eigen::ColVecs3d P;
    VDBParticleList_Pos(eigen::ColVecs3d P) : P(std::move(P)) {}
    VDBParticleList_Pos() = default;
    VDBParticleList_Pos(const VDBParticleList_Pos &) = default;
    VDBParticleList_Pos(VDBParticleList_Pos &&) = default;
    VDBParticleList_Pos &operator=(const VDBParticleList_Pos &) = default;
    VDBParticleList_Pos &operator=(VDBParticleList_Pos &&) = default;

    size_t size() const {
        return P.cols();
    }
    void getPos(size_t n, openvdb::Vec3R &xyz) const {
        auto p = P.col(n);
        xyz = openvdb::Vec3R(p.x(), p.y(), p.z());
    }
};

struct VDBParticleList_PosRad : public VDBParticleList_Pos {
    VDBParticleList_PosRad(eigen::ColVecs3d P, eigen::VecXd R)
      : VDBParticleList_Pos(std::move(P)), R(std::move(R)) {}
    VDBParticleList_PosRad() = default;
    VDBParticleList_PosRad(const VDBParticleList_PosRad &) = default;
    VDBParticleList_PosRad(VDBParticleList_PosRad &&) = default;
    VDBParticleList_PosRad &operator=(const VDBParticleList_PosRad &) = default;
    VDBParticleList_PosRad &operator=(VDBParticleList_PosRad &&) = default;
    eigen::VecXd R;
    void getPosRad(size_t n, openvdb::Vec3R &xyz, openvdb::Real &radius) const {
        getPos(n, xyz);
        radius = R(n);
    }
};

struct VDBParticleList_PosRadVec : public VDBParticleList_PosRad {
    VDBParticleList_PosRadVec(eigen::ColVecs3d P, eigen::VecXd R, eigen::ColVecs3d V)
      : VDBParticleList_PosRad(std::move(P), std::move(R)), V(std::move(V)) {}
    VDBParticleList_PosRadVec() = default;
    VDBParticleList_PosRadVec(const VDBParticleList_PosRadVec &) = default;
    VDBParticleList_PosRadVec(VDBParticleList_PosRadVec &&) = default;
    VDBParticleList_PosRadVec &operator=(const VDBParticleList_PosRadVec &) = default;
    VDBParticleList_PosRadVec &operator=(VDBParticleList_PosRadVec &&) = default;
    void getPosRadVel(size_t n, openvdb::Vec3R &xyz, openvdb::Real &radius, openvdb::Vec3R &vel) const {
        getPosRad(n, xyz, radius);
        auto v = V.col(n);
        vel = openvdb::Vec3R(v.x(), v.y(), v.z());
    }
    eigen::ColVecs3d V;
};

}// namespace balsa::geometry::point_cloud
