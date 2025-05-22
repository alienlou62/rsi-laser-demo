cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)

project(LaserDemo)

# Include the RapidSoftware.cmake file, which defines paths to RapidSoftware headers and libraries
include(${CMAKE_CURRENT_LIST_DIR}/cmake/RapidSoftware.cmake)

# Find OpenCV
find_package(PkgConfig REQUIRED)
pkg_check_modules(OpenCV REQUIRED opencv4)

# Find Pylon library
find_package(PYLON REQUIRED)

# Setup the target that doesn't use RTTasks
file(GLOB SRC_FILES src/*.cpp src/*.h)
add_executable(LaserDemo ${SRC_FILES})
setup_rmp_target(LaserDemo)

target_include_directories(LaserDemo PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${OpenCV_INCLUDE_DIRS}
  /opt/pylon/include
)

target_link_libraries(LaserDemo PUBLIC
  ${OpenCV_LIBRARIES}
  pylon::pylon
)

# Optional flags
target_compile_options(LaserDemo PRIVATE "-Wno-deprecated-enum-enum-conversion")

target_compile_definitions(LaserDemo PRIVATE
  CONFIG_FILE="${CMAKE_CURRENT_LIST_DIR}/acA640-300gc_22441779.pfs"
)

# Setup the target that uses RTTasks
add_subdirectory(RTTaskFunctions)
