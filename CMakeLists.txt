cmake_minimum_required(VERSION 3.18)

project(libavm-retom
  DESCRIPTION "RETOM-51 test-bench driver library for AVM-Energo projects"
  VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_BINARY_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Getting CMake system name
string(TOLOWER "${CMAKE_SYSTEM_NAME}" SYSTEM_NAME)

if(SYSTEM_NAME STREQUAL "linux")
    message(STATUS "${PROJECT_NAME} compiling for linux")
    add_compile_definitions(SYSTEM_TYPE=0)
elseif(SYSTEM_NAME STREQUAL "windows")
    message(STATUS "${PROJECT_NAME} compiling for windows")
    add_compile_definitions(SYSTEM_TYPE=1)
else()
    message(STATUS "${PROJECT_NAME} compiling for unknown")
    add_compile_definitions(SYSTEM_TYPE=2)
endif()

# Setup build type if it's not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
  message(STATUS "CMAKE_BUILD_TYPE is not specified. Using default: ${CMAKE_BUILD_TYPE}")
  # Possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

find_package(QT NAMES Qt6 Qt5)
find_package(Qt${QT_VERSION_MAJOR}
  COMPONENTS Core SerialPort REQUIRED
)

message(STATUS "QT version is: ${QT_VERSION}")

#------------------------------------------------------------------------------#
############################## Target definition ###############################
#------------------------------------------------------------------------------#
# Define target sources
file(GLOB_RECURSE cpp_sources ${PROJECT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE h_sources ${PROJECT_SOURCE_DIR}/include/*.h)

# Library building settings
if(BUILD_SHARED_LIBS)
    message(STATUS "${PROJECT_NAME} ${PROJECT_VERSION} SHARED LIBRARY")
    add_library(${PROJECT_NAME} SHARED ${h_sources} ${cpp_sources})
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DRETOM_EXPORTS)
    target_compile_definitions(${PROJECT_NAME} INTERFACE -DRETOM_IMPORTS)
else()
    message(STATUS "${PROJECT_NAME} ${PROJECT_VERSION} STATIC LIBRARY")
    add_library(${PROJECT_NAME} STATIC ${h_sources} ${cpp_sources})
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DHAVE_STATIC_BUILD)
endif()

# add synonym for retom
add_library(avm-energo::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# Set project version as target property
set_target_properties(${PROJECT_NAME} PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  PREFIX ""
)

target_link_libraries(
    ${PROJECT_NAME} PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::SerialPort
)

target_compile_definitions(${PROJECT_NAME} PUBLIC -DCMAKE_CONFIG)

# Using compile warnings to detect bugs and mistakes
if(SYSTEM_NAME STREQUAL "windows")
  target_compile_options(${PROJECT_NAME} PRIVATE)
else() # GCC or Clang
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wno-nullability-extension)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

# --- Install libs and binaries ---
set(PROJECT_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    CACHE STRING "Path to ${PROJECT_NAME} CMake files")

install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}_Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# --- Copying public headers with the directory structure ---
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" # source directory
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} # target directory (source included ${PROJECT_NAME} already)
        FILES_MATCHING # install only matched files
        PATTERN "*.h" # select header files
)

# create version file for cmake project
write_basic_package_version_file(${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION
    ${PROJECT_INSTALL_CMAKEDIR})

install(EXPORT ${PROJECT_NAME}_Targets
        FILE ${PROJECT_NAME}Targets.cmake
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION ${PROJECT_INSTALL_CMAKEDIR})

install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION ${PROJECT_INSTALL_CMAKEDIR})

set(VERSION_IN ${CMAKE_CURRENT_SOURCE_DIR}/VERSION.in)
set(VERSION_OUT ${CMAKE_CURRENT_SOURCE_DIR}/package/src/VERSION)
configure_file(${VERSION_IN} ${VERSION_OUT} @ONLY)

# Offline QtTest unit tests (ctest) and the manual live-hardware smoke tool -
# dev-only, not installed/packaged. Only built when this is the top-level
# project, so consumers pulling this in via add_subdirectory()/FetchContent
# don't get them too.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  option(RETOM_BUILD_TESTS "Build QtTest-based offline unit tests" ON)
  if(RETOM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
  endif()
  add_subdirectory(tools)
endif()
