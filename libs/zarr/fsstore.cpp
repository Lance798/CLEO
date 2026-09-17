/*
 * Copyright (c) 2024 MPI-M, Clara Bayley
 *
 *
 * ----- CLEO -----
 * File: fsstore.cpp
 * Project: zarr
 * Created Date: Monday 18th March 2024
 * Author: Clara Bayley (CB)
 * Additional Contributors: Tobias Kölling (TB)
 * -----
 * License: BSD 3-Clause "New" or "Revised" License
 * https://opensource.org/licenses/BSD-3-Clause
 * -----
 * File Description:
 * Functionality for class for writing memory in a a file system store under a given key.
 */

#include "./fsstore.hpp"

/**
 * @brief Write function called by StoreAccessor to write data to file system storage after the
 * data has been converted into a vector of unsigned integer types.
 *
 * This function can be used by a StoreAccessor object to write data represented as a vector of
 * unsigned integer types to the file system store under the specified key.
 *
 *
 * @param key The key under which the data will be stored in the file system store.
 * @param buffer A span representing the range of memory containing the unsigned bytes to be
 * written.
 * @return True if the write operation is successful, false otherwise.
 */
bool FSStore::write(std::string_view key, std::span<const uint8_t> buffer) const {
  auto path = basedir / key;
  auto mode = std::ios::out | std::ios::binary;
  std::ofstream out(path, mode);

  if (!out.good()) {
    /* The error_code overload, not the throwing one: with more than one rank
    every process creates the same array directories, and whichever loses the
    race between the failed open and create_directories used to abort the run
    with "cannot create directories: File exists". Another rank having made it
    already is the success case here, so ignore ec and just retry the open. */
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    out.open(path, mode);
  }

  if (!out.good()) {
    std::cout << "can't write to " << path << "\n";
    return false;
  }

  out.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
  return true;
}
