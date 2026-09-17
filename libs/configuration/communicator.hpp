/*
 * Copyright (c) 2025 MPI-M, Clara Bayley
 *
 *
 * ----- CLEO -----
 * File: communicator.hpp
 * Project: configuration
 * Created Date: Tuesday 06 May 2025
 * Author: Lakshmi Aparna Devulapalli (LAD)
 * Additional Contributors: Clara Bayley (CB)
 * -----
 * License: BSD 3-Clause "New" or "Revised" License
 * https://opensource.org/licenses/BSD-3-Clause
 * -----
 * File Description:
 * Header file for members of Config struct which determine CLEO's required configuration
 * parameters read from a config file.
 */

#ifndef LIBS_CONFIGURATION_COMMUNICATOR_HPP_
#define LIBS_CONFIGURATION_COMMUNICATOR_HPP_

#include <mpi.h>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "../cleoconstants.hpp"
#include "configuration/config.hpp"

class init_communicator {
  static int yac_comp_id;
  static MPI_Comm comm;
  bool yac_present;
  static int comm_size;
  static int my_rank;

 public:
  explicit init_communicator(int argc, char* argv[], const Config& config);
  ~init_communicator();
  static MPI_Comm get_communicator();
  static int get_yac_comp_id();
  /* Adopt a communicator created elsewhere.
  For CLEO embedded in a host model: the host owns MPI, so CLEO's constructor --
  which would call MPI_Init, and whose destructor calls MPI_Finalize -- must not
  run. Without this the static comm_size stays at its -1 sentinel and every
  get_comm_size() caller (the domain decomposition, superdroplet transport, the
  zarr chunk writer) silently works from nonsense. Pass the communicator the
  host's own domain decomposition is expressed in, which is not necessarily
  MPI_COMM_WORLD. */
  static void set_communicator(MPI_Comm communicator);

  static int get_comm_size();
  static int get_comm_rank();
};

#endif  // LIBS_CONFIGURATION_COMMUNICATOR_HPP_
