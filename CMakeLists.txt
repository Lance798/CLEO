# run cmake with " cmake -S [source dir of targerts] -B [build dir for makefile] "
# e.g. could run cmake with these compiler options:
# cmake -S ./ -B ./build -DCMAKE_CXX_COMPILER=mpic++ -DCMAKE_C_COMPILER=mpicc -DKokkos_ARCH_NATIVE=ON -DKokkos_ENABLE_SERIAL=ON -DKokkos_ENABLE_OPENMP=ON

# set cmake version
cmake_minimum_required(VERSION 3.18.0)
# cmake_minimum_required(VERSION 3.21.1) ### if using Kokkos with nvc++ compiler on Levante

project(CLEO
  # note: this version is what find_package(CLEO <version>) checks against, so it needs bumping
  # alongside the git tag that cog creates (see cog.toml)
  VERSION 0.69.1
  LANGUAGES CXX C
  DESCRIPTION "Cleo by Clara Bayley and other developers"
)

# CLEO may be consumed as a subproject (e.g. via add_subdirectory or FetchContent). Detect that
# here so CLEO can avoid polluting a parent project with its targets and its optional components.
# (equivalent to PROJECT_IS_TOP_LEVEL, which requires cmake 3.21 whereas CLEO requires only 3.18)
if(CMAKE_SOURCE_DIR STREQUAL CLEO_SOURCE_DIR)
  set(CLEO_IS_TOP_LEVEL ON)
else()
  set(CLEO_IS_TOP_LEVEL OFF)
endif()
message(STATUS "CLEO is the top level project: ${CLEO_IS_TOP_LEVEL}")

# as a subproject CLEO's optional components are off by default (the python bindings in particular
# fetch pybind11 at configure time), but an explicit -DCLEO_NO_<X> from the parent still wins
if(NOT CLEO_IS_TOP_LEVEL)
  foreach(cleo_optional_component CLEO_NO_EXAMPLES CLEO_NO_ROUGHPAPER CLEO_NO_PYBINDINGS)
    if(NOT DEFINED ${cleo_optional_component})
      set(${cleo_optional_component} ON)
    endif()
  endforeach()
endif()

# CLEO can install itself so that another project can use find_package(CLEO) instead of building
# CLEO as a subproject. Off by default when CLEO is itself a subproject, because a parent project's
# `make install` would otherwise install CLEO alongside the parent.
include(GNUInstallDirs)
option(CLEO_ENABLE_INSTALL "Generate install rules and a CLEOConfig.cmake for CLEO" ${CLEO_IS_TOP_LEVEL})
message(STATUS "CLEO install rules CLEO_ENABLE_INSTALL=${CLEO_ENABLE_INSTALL}")

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

# install yaml-cpp for project, unless a parent project already provides it (fetching it twice
# would be a duplicate target error)
if(TARGET yaml-cpp::yaml-cpp)
  message(STATUS "Using yaml-cpp target already provided by the parent project")
elseif(TARGET yaml-cpp)
  # some parent projects (e.g. EKAT) provide only the unqualified target, but CLEO links the
  # namespaced one, so alias it here
  message(STATUS "Using unqualified yaml-cpp target already provided by the parent project")
  add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
else()
  set(yamlcpppath ${CLEO_SOURCE_DIR}/extern/yaml-cpp)
  message(STATUS "Using yaml-cpp installation from: ${yamlcpppath}")
  add_subdirectory(${yamlcpppath} ${CLEO_BINARY_DIR}/yaml-cpp)
endif()

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

# install rules and package configuration, so that another project can do
#   find_package(CLEO REQUIRED)
#   target_link_libraries(myapp PRIVATE CLEO::runcleo CLEO::configuration [...])
if(CLEO_ENABLE_INSTALL)
  include(CMakePackageConfigHelpers)

  # collect the CLEO libraries which this particular configuration builds (which coupled dynamics
  # and which domain are included depends on CLEO_COUPLED_DYNAMICS and CLEO_DOMAIN)
  set(cleo_known_libraries
    configuration initialise superdrops collisions gridboxes runcleo zarr observers sdmmonitor
    cartesiandomain movement coupldyn_fromfile coupldyn_cvode coupldyn_yac)
  set(cleo_exported_libraries "")
  foreach(cleo_library ${cleo_known_libraries})
    if(TARGET ${cleo_library})
      list(APPEND cleo_exported_libraries ${cleo_library})
      # several CLEO libraries are added with EXCLUDE_FROM_ALL, which would leave `make install`
      # with no archive to install for them
      set_target_properties(${cleo_library} PROPERTIES EXCLUDE_FROM_ALL FALSE)
    endif()
  endforeach()
  message(STATUS "CLEO installing libraries: ${cleo_exported_libraries}")

  install(TARGETS ${cleo_exported_libraries}
    EXPORT CLEOTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  # CLEO's headers sit next to its sources and are included relative to libs/, e.g. as
  # "configuration/config.hpp" or "../cleoconstants.hpp", so that layout is preserved on install
  install(DIRECTORY ${CLEO_SOURCE_DIR}/libs/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/CLEO
    FILES_MATCHING
      PATTERN "*.hpp"
      PATTERN "cleo_python_bindings" EXCLUDE)

  install(EXPORT CLEOTargets
    FILE CLEOTargets.cmake
    NAMESPACE CLEO::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CLEO)

  # CLEOConfig.cmake needs CLEO's Find modules to re-find YAC for a YAC enabled installation
  if(CLEO_ENABLE_YAC)
    install(DIRECTORY ${CLEO_SOURCE_DIR}/libs/coupldyn_yac/cmake/
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CLEO/modules
      FILES_MATCHING PATTERN "*.cmake")
  endif()

  configure_package_config_file(
    ${CLEO_SOURCE_DIR}/cmake/CLEOConfig.cmake.in
    ${CLEO_BINARY_DIR}/CLEOConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CLEO)
  write_basic_package_version_file(
    ${CLEO_BINARY_DIR}/CLEOConfigVersion.cmake
    VERSION ${CLEO_VERSION}
    COMPATIBILITY SameMinorVersion)
  install(FILES
    ${CLEO_BINARY_DIR}/CLEOConfig.cmake
    ${CLEO_BINARY_DIR}/CLEOConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CLEO)
endif()

# CLEO's clean-up targets are only added when CLEO is the top level project: their names are
# global, so as a subproject they would collide with a parent project's own targets
if(CLEO_IS_TOP_LEVEL)

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

endif()
