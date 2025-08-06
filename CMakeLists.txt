cmake_minimum_required(VERSION 3.12)
project(LaserDemo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include our RapidSoftware utilities
include(cmake/RapidSoftware.cmake)

# Find OpenCV
find_package(PkgConfig REQUIRED)
pkg_check_modules(OpenCV REQUIRED opencv4)

# Find Pylon library
find_package(PYLON REQUIRED)
set(PYLON_INCLUDE_DIRS /opt/pylon/include)

# Output the task library to the RMP directory where RTTasks will look for it
set(RTTASK_FUNCTIONS_OUTPUT_DIR ${RMP_DIR})

# Setup the RTTask functions library

# Grab all the source and header files in the rttasks directory
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_SOURCE_DIR}/rttasks/*.cpp")
file(GLOB_RECURSE HEADER_FILES "${CMAKE_SOURCE_DIR}/rttasks/*.h")

# Create a library target for the RTTask functions
add_library(RTTaskFunctions SHARED ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(RTTaskFunctions PRIVATE
  ${CMAKE_SOURCE_DIR}/rttasks
  ${CMAKE_SOURCE_DIR}/rttasks/include
  ${OpenCV_INCLUDE_DIRS}
  ${PYLON_INCLUDE_DIRS}
)
target_link_libraries(RTTaskFunctions PUBLIC 
  ${OpenCV_LIBRARIES}
  pylon::pylon
)
target_compile_definitions(RTTaskFunctions PRIVATE CONFIG_FILE="/etc/laser_demo/acA640-300gc_22441779.pfs")
target_compile_options(RTTaskFunctions PRIVATE "-Wno-deprecated-enum-enum-conversion")
set_target_properties(RTTaskFunctions PROPERTIES 
  COMPILE_WARNING_AS_ERROR ON
  ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  LIBRARY_OUTPUT_DIRECTORY_RELEASE ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  LIBRARY_OUTPUT_DIRECTORY_DEBUG ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${RTTASK_FUNCTIONS_OUTPUT_DIR}
)
setup_rmp_target(RTTaskFunctions)
