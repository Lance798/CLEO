# run cmake with " cmake -S [source dir of targerts] -B [build dir for makefile] "
# e.g. could run cmake with these compiler options:
# cmake -S ./ -B ./build -DCMAKE_CXX_COMPILER=mpic++ -DCMAKE_C_COMPILER=mpicc -DKokkos_ARCH_NATIVE=ON -DKokkos_ENABLE_SERIAL=ON -DKokkos_ENABLE_OPENMP=ON

# set cmake version
cmake_minimum_required(VERSION 3.18.0)
# cmake_minimum_required(VERSION 3.21.1) ### if using Kokkos with nvc++ compiler on Levante

project(CLEO
  LANGUAGES CXX C
  DESCRIPTION "Cleo by Clara Bayley and other developers"
)

find_package(MPI REQUIRED COMPONENTS C)

message(STATUS "CLEO source from CLEO_SOURCE_DIR: ${CLEO_SOURCE_DIR}")
message(STATUS "CLEO build in CLEO_BINARY_DIR: ${CLEO_BINARY_DIR}")

### ensure these directories exist (it's a good idea for later use)
file(MAKE_DIRECTORY ${CLEO_BINARY_DIR}/tmp)
file(MAKE_DIRECTORY ${CLEO_BINARY_DIR}/bin)
file(MAKE_DIRECTORY ${CLEO_BINARY_DIR}/share)

# ensure C++ compiler uses certain settings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
set(CMAKE_CXX_STANDARD "20")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CXX_EXTENSIONS ON)

find_package(Kokkos CONFIG)
if(Kokkos_FOUND)
  message(STATUS "Found Kokkos: ${Kokkos_DIR} (version \"${Kokkos_VERSION}\")")
else()
  message(FATAL_ERROR "Couldn't find Kokkos." )
endif()

# install yaml-cpp for project
set(yamlcpppath ${CLEO_SOURCE_DIR}/extern/yaml-cpp)
message(STATUS "Using yaml-cpp installation from: ${yamlcpppath}")
add_subdirectory(${yamlcpppath} ${CLEO_BINARY_DIR}/yaml-cpp)

# YAC is an optional dependency of CLEO. It is mandatory for CLEO's YAC coupled dynamics, but
# otherwise CLEO is built without it (a config file requesting YAC dynamics is then rejected
# at runtime). Set -DCLEO_ENABLE_YAC=ON to build YAC support in regardless of the coupled dynamics.
option(CLEO_ENABLE_YAC "Build CLEO with YAC support (requires CLEO_YAC_ROOT and CLEO_YAXT_ROOT)" OFF)
if("${CLEO_COUPLED_DYNAMICS}" STREQUAL "yac" OR "${CLEO_COUPLED_DYNAMICS}" STREQUAL "all")
  set(CLEO_ENABLE_YAC ON) # YAC is mandatory for these coupled dynamics
endif()
message(STATUS "CLEO with YAC support CLEO_ENABLE_YAC=${CLEO_ENABLE_YAC}")

# print default compiler flags
message(STATUS "CLEO primary CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")

# add directories of CLEO libray and main program
add_subdirectory(libs)

# add directories for specific examples of CLEO
if(CLEO_NO_EXAMPLES)
  message(STATUS "CLEO excluding examples CLEO_NO_EXAMPLES=${CLEO_NO_EXAMPLES}")
else()
  add_subdirectory(examples EXCLUDE_FROM_ALL)
endif()

# add directory for roughpaper / quick tests
if(CLEO_NO_ROUGHPAPER)
  message(STATUS "CLEO excluding roughpaper CLEO_NO_ROUGHPAPER=${CLEO_NO_ROUGHPAPER}")
else()
  add_subdirectory(roughpaper EXCLUDE_FROM_ALL)
endif()

# "make distclean" / "make dist-clean" target to perform `make clean`
# and then remove generated build files and third-party build dirs
add_custom_target(distclean
  COMMAND "${CMAKE_MAKE_PROGRAM}" clean
  COMMAND ${CMAKE_COMMAND} -E echo "CLEO: performing make clean and distclean in ${CLEO_BINARY_DIR}"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CLEO_BINARY_DIR}/CMakeFiles"
  COMMAND ${CMAKE_COMMAND} -E remove "${CLEO_BINARY_DIR}/CMakeCache.txt"
  COMMAND ${CMAKE_COMMAND} -E remove "${CLEO_BINARY_DIR}/cmake_install.cmake"
  COMMAND ${CMAKE_COMMAND} -E remove "${CLEO_BINARY_DIR}/Makefile"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CLEO_BINARY_DIR}/kokkos"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CLEO_BINARY_DIR}/yaml-cpp"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CLEO_BINARY_DIR}/_deps"
  WORKING_DIRECTORY "${CLEO_BINARY_DIR}"
  COMMENT "distclean: perform `make clean` and then remove CMake cache/files, vendored build output"
)
# add an alternate name for convenience
add_custom_target("dist-clean"
  COMMAND ${CMAKE_COMMAND} --build "${CLEO_BINARY_DIR}" --target distclean
)

# "make wipeclean" / "make wipe-clean" target to remove any extra files from CLEO output in
# tmp, bin and share directories, and then perform actions of distclean target
add_custom_target(wipeclean
  COMMAND ${CMAKE_COMMAND} -E echo "CLEO: performing wipeclean and distclean in ${CLEO_BINARY_DIR}"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CLEO_BINARY_DIR}/tmp"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CLEO_BINARY_DIR}/tmp"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CLEO_BINARY_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CLEO_BINARY_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CLEO_BINARY_DIR}/share"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CLEO_BINARY_DIR}/share"
  COMMAND ${CMAKE_COMMAND} --build "${CLEO_BINARY_DIR}" --target distclean
  WORKING_DIRECTORY "${CLEO_BINARY_DIR}"
  COMMENT "wipeclean: remove all files in tmp, bin and share, then perform actions of distclean target"
)
# add an alternate name for convenience
add_custom_target("wipe-clean"
  COMMAND ${CMAKE_COMMAND} --build "${CLEO_BINARY_DIR}" --target wipeclean
)
