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
set(PYLON_INCLUDE_DIRS /opt/pylon/include)

set(COMMON_SRC_FILES
  ${CMAKE_CURRENT_LIST_DIR}/src/camera_helpers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/rmp_helpers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/misc_helpers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/image_processing.cpp
)

# Setup the target that doesn't use RTTasks
add_executable(LaserDemo 
  ${CMAKE_CURRENT_LIST_DIR}/src/demo_main.cpp
  ${COMMON_SRC_FILES}
)
setup_rmp_target(LaserDemo)

target_include_directories(LaserDemo PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${OpenCV_INCLUDE_DIRS}
  ${PYLON_INCLUDE_DIRS}
)

target_link_libraries(LaserDemo PUBLIC
  ${OpenCV_LIBRARIES}
  pylon::pylon
)

target_compile_definitions(LaserDemo PRIVATE
  CONFIG_FILE="${CMAKE_CURRENT_LIST_DIR}/acA640-300gc_22441779.pfs"
)

target_compile_options(LaserDemo PRIVATE "-Wno-deprecated-enum-enum-conversion")

# Setup the target that uses RTTasks
add_subdirectory(RTTaskFunctions)
add_executable(LaserDemoRTTasks 
  ${CMAKE_CURRENT_LIST_DIR}/src/rttasks_demo_main.cpp
  ${COMMON_SRC_FILES}
)
setup_rmp_target(LaserDemoRTTasks)

target_include_directories(LaserDemoRTTasks PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${OpenCV_INCLUDE_DIRS}
  ${PYLON_INCLUDE_DIRS}
)

target_link_libraries(LaserDemoRTTasks PUBLIC
  ${OpenCV_LIBRARIES}
  pylon::pylon
)

target_compile_definitions(LaserDemoRTTasks PRIVATE
  CONFIG_FILE="${CMAKE_CURRENT_LIST_DIR}/acA640-300gc_22441779.pfs"
)

target_compile_options(LaserDemoRTTasks PRIVATE "-Wno-deprecated-enum-enum-conversion")