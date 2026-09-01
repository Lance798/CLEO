# set cmake version
if(NOT DEFINED CMAKE_MINIMUM_REQUIRED_VERSION)
  cmake_minimum_required(VERSION 3.18.0)
endif()

# subdirectories for specific examples of CLEO
add_subdirectory(adiabaticparcel/src EXCLUDE_FROM_ALL)
add_subdirectory(boxmodelcollisions EXCLUDE_FROM_ALL)
add_subdirectory(constthermo2d/src EXCLUDE_FROM_ALL)
add_subdirectory(divfreemotion/src EXCLUDE_FROM_ALL)
add_subdirectory(eurec4a1d/src EXCLUDE_FROM_ALL)
add_subdirectory(rainshaft1d/src EXCLUDE_FROM_ALL)
add_subdirectory(fromfile/src EXCLUDE_FROM_ALL)
add_subdirectory(fromfile_irreg/src EXCLUDE_FROM_ALL)

# bubble3d is coupled to dynamics via YAC, so it can only be built if coupldyn_yac exists
if(TARGET coupldyn_yac)
  add_subdirectory(bubble3d/src EXCLUDE_FROM_ALL)
else()
  message(STATUS "CLEO excluding bubble3d example because CLEO is built without YAC coupled dynamics")
endif()
