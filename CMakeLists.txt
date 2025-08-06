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

# Create a library target for the RTTask functions
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_SOURCE_DIR}/rttasks/*.cpp")
file(GLOB_RECURSE HEADER_FILES "${CMAKE_SOURCE_DIR}/rttasks/*.h")
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
target_compile_definitions(RTTaskFunctions PRIVATE CONFIG_FILE="/etc/laser_demo/camera.pfs")
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

# Copy config files to /etc/laser_demo after build
set(SOURCE_CONFIG_DIR ${CMAKE_SOURCE_DIR}/config)
set(TARGET_CONFIG_DIR /etc/laser_demo)
add_custom_command(
  TARGET RTTaskFunctions POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${TARGET_CONFIG_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${SOURCE_CONFIG_DIR} ${TARGET_CONFIG_DIR}
  COMMENT "Copying config files to ${TARGET_CONFIG_DIR}"
)