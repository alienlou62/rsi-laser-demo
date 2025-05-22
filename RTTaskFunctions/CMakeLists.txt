cmake_minimum_required(VERSION 3.15)

# Enable C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create the project
project(RTTaskFunctionsTemplate)

# The directory of the cmake helper files
set(CMAKE_HELPERS_DIR ${CMAKE_CURRENT_LIST_DIR}/../cmake)

# Include the RapidSoftware.cmake file, which defines paths to RapidSoftware headers and libraries
# and includes the helper functions for setting up RMP targets and INtime builds
include(${CMAKE_HELPERS_DIR}/RapidSoftware.cmake)

# Set the output directory to the RMP directory, which is the default location
# that RTTasks will look for the library. If you want to change this, then you
# will need to pass the library path when submitting a RTTask.
set(RTTASK_FUNCTIONS_OUTPUT_DIR ${RMP_DIR})

# If we're on Windows, then configure the INtime build
if (WINDOWS_BUILD)
  configure_cmake_for_intime(${CMAKE_SOURCE_DIR})
endif()

# Setup the RTTask functions library

# Grab all the source and header files in the src directory
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
file(GLOB_RECURSE HEADER_FILES "${CMAKE_CURRENT_LIST_DIR}/src/*.h")

# Create a library target for the RTTask functions
add_library(RTTaskFunctions SHARED ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(RTTaskFunctions PUBLIC 
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${CMAKE_CURRENT_LIST_DIR}/../include
  ${OpenCV_INCLUDE_DIRS}
  ${PYLON_INCLUDE_DIRS}
)

target_link_libraries(RTTaskFunctions PUBLIC
  ${OpenCV_LIBRARIES}
  pylon::pylon
)

target_compile_options(RTTaskFunctions PRIVATE "-Wno-deprecated-enum-enum-conversion")

# Perform all the standard setup for a target that uses RMP
# This includes setting the RMP include directories and linking to the RMP library
setup_rmp_target(RTTaskFunctions)

# Set the output directory for the library. This is where the library will be
# copied to when the project is built.
set_target_properties(RTTaskFunctions PROPERTIES 
  COMPILE_WARNING_AS_ERROR ON
  ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  LIBRARY_OUTPUT_DIRECTORY_RELEASE ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  LIBRARY_OUTPUT_DIRECTORY_DEBUG ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${RTTASK_FUNCTIONS_OUTPUT_DIR}
)

# If we're on Windows, then we need create a target for calling the INtime build
if (WINDOWS_BUILD)
  configure_intime_target(RTTaskFunctions)
endif()
