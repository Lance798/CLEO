/*
 * Copyright (c) 2024 MPI-M, Clara Bayley
 *
 *
 * ----- CLEO -----
 * File: cartesian_decomposition.hpp
 * Project: cartesiandomain
 * Created Date: Tuesday 30th July 2023
 * Author: Wilton J. Loch
 * Additional Contributors:
 * -----
 * License: BSD 3-Clause "New" or "Revised" License
 * https://opensource.org/licenses/BSD-3-Clause
 * -----
 * File Description:
 * Class and suporting functions to perform domain decomposition
 * in a cartesian domain
 */

#ifndef LIBS_CARTESIANDOMAIN_CARTESIAN_DECOMPOSITION_HPP_
#define LIBS_CARTESIANDOMAIN_CARTESIAN_DECOMPOSITION_HPP_

#include <Kokkos_Core.hpp>
#include <mpi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

#include "../cleoconstants.hpp"
#include "cartesiandomain/domainboundaries.hpp"
#include "configuration/communicator.hpp"
#include "initialise/gbx_bounds_from_binary.hpp"

/* A domain decomposition computed by someone else.
 *
 * CLEO normally chooses its own factorization of the process count. When CLEO is
 * embedded in a host model that has already decomposed the same domain, the two
 * must agree gridbox for gridbox, and reproducing the host's choice inside CLEO
 * is not generally possible: a host built on MPI_Cart_create with reorder=true
 * gets a rank numbering that is implementation-defined, so there is no formula
 * to copy. Passing the answer in sidesteps that entirely.
 *
 * origins and sizes are indexed by rank in CLEO's communicator, and are in
 * CLEO's dimension order (z, x, y) and in units of gridboxes. decomposition is
 * how many parts each dimension is cut into; its product must be the number of
 * processes, and the partitions must tile the domain exactly. */
struct SuppliedDecomposition {
  std::array<size_t, 3> decomposition;
  std::vector<std::array<size_t, 3>> origins;
  std::vector<std::array<size_t, 3>> sizes;
};

class CartesianDecomposition {
 private:
  int my_rank;
  // Number of dimensions of the global domain
  std::vector<size_t> ndims;
  // Global origins of all partitions
  std::vector<std::array<size_t, 3>> partition_origins;
  // Sizes of all partitions
  std::vector<std::array<size_t, 3>> partition_sizes;

  // Geometrical coordinates of the local partition begin and end
  std::array<double, 3> partition_begin_coordinates;
  std::array<double, 3> partition_end_coordinates;

  // Geometric bounds of a gridbox in z, x, y directions
  std::vector<std::vector<double>> gridbox_bounds;

  // Geometric bounds of the entire domain
  // First Index 0 : Lower bounds in z, x, y directions
  // First Index 1 : Upper bounds in z, x, y directions
  std::array<std::array<double, 3>, 2> domain_bounds;

  // Behavior of each dimension, being either periodic or finite
  std::array<size_t, 3> dimension_bound_behavior;

  // Which process neighbors the current one in each direction
  // (it can also be the same as the local one)
  std::map<std::array<int, 3>, int> neighboring_processes;

  // Domain decomposition factors for each dimension
  // (i.e. in how many parts each dimension is divided)
  std::array<size_t, 3> decomposition;
  // Number of local gridboxes
  size_t total_local_gridboxes;

  /* Device-resident copy of exactly what get_local_bounding_gridbox_index reads.
   *
   * That function runs inside MoveSupersInGridboxesFunctor, i.e. on the GPU. The
   * members above are host containers, so reading partition_sizes[my_rank],
   * gridbox_bounds[d][i] or neighboring_processes.at(dir) from device code
   * follows a host heap pointer and faults -- compute-sanitizer reports an
   * "Invalid __global__ read" tens of GB past the nearest allocation, identical
   * for every thread because the bad pointer is one scalar in the captured
   * object. Single-rank runs never hit it: is_decomp is false there, so
   * CartesianMaps takes the no-decomposition branch instead.
   *
   * std::array members (domain_bounds, dimension_bound_behavior) need no mirror:
   * they hold their data inline, so a by-value capture carries the values
   * themselves rather than a pointer.
   *
   * gridbox_bounds is ragged -- dimension d holds partition_size[d] + 1 bounds --
   * so it is flattened into one View with per-dimension start offsets.
   *
   * SharedSpace, not the default device space: sendrecv_supers calls this same
   * function from the HOST when placing received superdroplets, and a CudaSpace
   * view there aborts with "attempt to access inaccessible memory space". The
   * array is a few kB (one bound per local gridbox per dimension), so paying for
   * managed memory is cheaper than carrying a second mirror and branching on the
   * execution space at every access. */
  Kokkos::View<double *, Kokkos::SharedSpace> d_gridbox_bounds;
  Kokkos::Array<size_t, 3> d_gridbox_bounds_begin;
  Kokkos::Array<size_t, 3> d_local_partition_size;
  // Indexed by (dz + 1) * 9 + (dx + 1) * 3 + (dy + 1) for a direction in
  // {-1,0,1}^3. The {0,0,0} slot is never read and stays -1.
  Kokkos::Array<int, 27> d_neighbour_of_direction;

  // Fills the four members above from the host ones. Must run after
  // set_gridbox_bounds and calculate_neighboring_processes.
  void build_device_mirror();

  // Device-callable equivalent of the free binary_search below, reading the
  // flattened bounds instead of the vector of vectors.
  KOKKOS_FUNCTION int bounding_gridbox_in_dimension(double coordinate, int dimension) const;

  // Slice indices of every rank, and the reverse lookup. Both are empty unless a
  // SuppliedDecomposition was used: the arithmetic in
  // get_slice_indices_from_partition assumes CLEO's own rank ordering, which a
  // supplied decomposition is free not to follow.
  std::vector<std::array<int, 3>> slice_of_rank;
  std::map<std::array<int, 3>, int> rank_of_slice;

  // Derives slice_of_rank / rank_of_slice from partition_origins
  void index_partitions_into_slices();

  // Fill the partition_begin_coordinates and partition_end_coordinates arrays
  void calculate_partition_coordinates();

  // Fills the neighboring_processes array
  void calculate_neighboring_processes();

 public:
  CartesianDecomposition();
  ~CartesianDecomposition();

  // Creates the decomposition
  bool create(std::vector<size_t> ndims, GbxBoundsFromBinary gfb);

  // Creates the decomposition from one the caller already computed, instead of
  // factorizing the process count. See SuppliedDecomposition above.
  bool create(std::vector<size_t> ndims, GbxBoundsFromBinary gfb,
              const SuppliedDecomposition& supplied);

  // Local and global amount of gridboxes
  size_t get_total_local_gridboxes() const;
  size_t get_total_global_gridboxes() const;

  // Get the origin and size of local partition in terms of number of gridboxes
  std::array<size_t, 3> get_local_partition_origin() const;
  std::array<size_t, 3> get_local_partition_size() const;

  // Get partition index and partition coordinates
  int get_partition_index_from_slice(std::array<int, 3> slice_indices) const;
  std::array<int, 3> get_slice_indices_from_partition(int partition_index) const;
  // Checks whether a coordinate is bounded by one specific partition
  bool check_indices_inside_partition(std::array<size_t, 3> indices, int partition_index) const;

  // Gridbox related subroutines
  int local_to_global_gridbox_index(size_t local_gridbox_index, int process = -1) const;
  int global_to_local_gridbox_index(size_t global_gridbox_index) const;
  int get_gridbox_owner_process(size_t global_gridbox_index) const;
  KOKKOS_FUNCTION unsigned int get_local_bounding_gridbox_index(
      std::array<double, 3>& coordinates) const;
  // void set_gridbox_size(double z_size, double x_size, double y_size);
  void set_gridbox_bounds(GbxBoundsFromBinary gfb);
  // Sets the behavior of all dimensions
  void set_dimensions_bound_behavior(std::array<size_t, 3> behaviors);
};

// Given the global domain, a global decomposition and a partition index,
// returns the partition origin and size
void construct_partition(const std::vector<size_t> ndims, std::vector<size_t> decomposition,
                         int partition_index, std::array<size_t, 3>& partition_origin,
                         std::array<size_t, 3>& partition_size);

// Adds all permutations of a particular decomposition and removes the ones that
// do not fit the global dimension sizes
void permute_and_trim_factorizations(std::vector<std::vector<size_t>>& factors,
                                     const std::vector<size_t> ndims);

// Finds the best decomposition given by the most even division of gridboxes among processes
int find_best_decomposition(std::vector<std::vector<size_t>>& factors,
                            const std::vector<size_t> ndims);

// Functions for getting indexes and coordinates in an arbitrary 3D gridbox domain
size_t get_index_from_coordinates(const std::vector<size_t>& ndims, const size_t k, const size_t i,
                                  const size_t j);
std::array<size_t, 3> get_coordinates_from_index(const std::vector<size_t>& ndims,
                                                 const size_t index);

// Support functions
std::vector<std::vector<size_t>> factorize(int n);
void factorize_helper(int n, int start, std::vector<size_t>& current,
                      std::vector<std::vector<size_t>>& result);
void heap_permutation(std::vector<std::vector<size_t>>& results, std::vector<size_t> arr, int size);
int get_multiplications_to_turn_int(double entry_value);
int binary_search(std::array<double, 3>& coordinates, int dimension,
                  std::array<size_t, 3> partition_size,
                  std::vector<std::vector<double>> gridbox_bounds);
#endif  // LIBS_CARTESIANDOMAIN_CARTESIAN_DECOMPOSITION_HPP_
