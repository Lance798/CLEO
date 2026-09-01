# set cmake version
if(NOT DEFINED CMAKE_MINIMUM_REQUIRED_VERSION)
  cmake_minimum_required(VERSION 3.18.0)
endif()

# fetch and make yaml-cpp library available
include(FetchContent)

# yaml-cpp is part of CLEO's public interface, so if CLEO is installed then the yaml-cpp it
# vendored has to be installed and exported too, otherwise install(EXPORT CLEOTargets) fails on
# a dependency which is in no export set (yaml-cpp only installs itself if it is the main project)
if(CLEO_ENABLE_INSTALL)
  set(YAML_CPP_INSTALL ON CACHE BOOL "" FORCE)
endif()

# yaml-cpp's parse tools default to ON and CLEO has no use for them
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  yaml-cpp
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.9.0.tar.gz
  GIT_TAG 56e3bb550c91fd7005566f19c079cb7a503223cf
)
FetchContent_MakeAvailable(yaml-cpp)

message(STATUS "yaml-cpp installation in: ${CMAKE_CURRENT_BINARY_DIR}")
