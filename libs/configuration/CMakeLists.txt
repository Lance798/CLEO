# set cmake version
if(NOT DEFINED CMAKE_MINIMUM_REQUIRED_VERSION)
  cmake_minimum_required(VERSION 3.18.0)
endif()

# set library name and print directory of this CMakeLists.txt (source directory of library)
set(LIBNAME "configuration")
message(STATUS "CLEO using ${LIBNAME} with LIBRARY_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# explicitly set library executables path to /lib in top level of build tree
set(LIB_BINARY_DIR ${CLEO_BINARY_DIR}/lib)

enable_language(C)
message(STATUS "CLEO ${LIBNAME} enabling C language")

# require MPI explicitly for this library
find_package(MPI REQUIRED COMPONENTS C)

# YAC is an optional dependency of this library, see CLEO_ENABLE_YAC in the top level CMakeLists.txt
if(CLEO_ENABLE_YAC)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CLEO_YAC_MODULE_PATH})
  message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
  message(STATUS "Using YAC pre-installed in: ${CLEO_YAC_ROOT}")
  message(STATUS "Using YAXT pre-installed in: ${CLEO_YAXT_ROOT}")
  set(YAC_ROOT ${CLEO_YAC_ROOT})
  set(YAXT_ROOT ${CLEO_YAXT_ROOT})
  find_package(YAC REQUIRED)
else()
  message(STATUS "CLEO ${LIBNAME} building without YAC (CLEO_ENABLE_YAC=${CLEO_ENABLE_YAC})")
endif()

# Add executables and create library target
set(SOURCES
"copyfiles2txt.cpp"
"optional_config_params.cpp"
"required_config_params.cpp"
"communicator.cpp"
)
# must use STATIC (not(!) SHARED) lib for linking to executable if build is CUDA enabled with Kokkos
add_library("${LIBNAME}" STATIC ${SOURCES})

# namespaced alias so a project consuming CLEO can link CLEO::${LIBNAME} rather than the
# unqualified target name, which is generic enough to clash in a parent project
add_library("CLEO::${LIBNAME}" ALIAS "${LIBNAME}")

# Add directories for target library
#target_include_directories("${LIBNAME}"" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(${LIBNAME} PUBLIC
  "$<BUILD_INTERFACE:${CLEO_SOURCE_DIR}/libs>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/CLEO>")
target_include_directories(${LIBNAME} PRIVATE ${MPI_INCLUDE_PATH})

# Link libraries to target library
target_link_libraries(${LIBNAME} PUBLIC Kokkos::kokkos yaml-cpp::yaml-cpp MPI::MPI_C)

# Add YAC to target library (only if CLEO is built with YAC support)
if(CLEO_ENABLE_YAC)
  target_include_directories(${LIBNAME} PRIVATE ${YAC_C_INCLUDE_DIR}) # YAC source directory
  target_compile_definitions(${LIBNAME} PUBLIC CLEO_ENABLE_YAC)
  target_link_libraries(${LIBNAME} PRIVATE YAC::YAC)
endif()

# set specific C++ compiler options for target (optional)
#target_compile_options("${LIBNAME}" PRIVATE)

# set C++ properties for target
set_target_properties("${LIBNAME}" PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${LIB_BINARY_DIR}
  CMAKE_CXX_STANDARD_REQUIRED ON
  CMAKE_CXX_EXTENSIONS ON
  CXX_STANDARD 20)
