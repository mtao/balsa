#if !defined(BALSA_GEOMETRY_POINT_CLOUD_BRIDSON_POISSON_DISK_SAMPLING_HPP)
#define BALSA_GEOMETRY_POINT_CLOUD_BRIDSON_POISSON_DISK_SAMPLING_HPP
// Bridson's Poisson Disk Sampling
//
// NOTE: This file is a legacy stub from the mtao:: codebase. It depends on
// mtao::geometry::grid infrastructure (GridData, Grid, multi_loop) that has
// not yet been ported to balsa. The algorithm itself is correct but cannot
// compile until the grid utilities are available.
//
// TODO: Port mtao::geometry::grid::{Grid, GridData, utils::multi_loop} to
// balsa::geometry::grid, then rewrite this file to use balsa:: types.

namespace balsa::geometry::point_cloud {

// Generates blue-noise distributed points within a bounding box using
// Bridson's Poisson disk sampling algorithm.
//
// Parameters:
//   bounding_box   - Axis-aligned bounding box to sample within
//   radius         - Minimum distance between samples
//   insert_attempts - Number of candidate points per active sample (default 10)
//
// Returns:
//   ColVectors<T, D> with N columns, one per accepted sample point.
//
// Requires: balsa::geometry::grid infrastructure (not yet ported)

}// namespace balsa::geometry::point_cloud
#endif
