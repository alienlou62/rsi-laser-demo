cmake_minimum_required(VERSION 3.15)

# Enable C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create the project
project(SampleApps_RTTaskFunctions)

# The root directory of the C++ sample apps and the cmake helpers
set(SAMPLE_APPS_CPP_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(CMAKE_HELPERS_DIR "${SAMPLE_APPS_CPP_ROOT_DIR}/cmake")

# Include the SampleAppsCPP.cmake file, which defines paths to necessary headers
# and libraries as well as a functions to create sample app targets
include("${CMAKE_HELPERS_DIR}/SampleAppsCPP.cmake")

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
add_library(RealTimeTasks_RTTaskFunctions SHARED ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(RealTimeTasks_RTTaskFunctions PUBLIC 
  ${CMAKE_CURRENT_LIST_DIR}/src
)

# Perform all the standard setup for a target that uses RMP
# This includes setting the RMP include directories and linking to the RMP library
setup_rmp_target(RealTimeTasks_RTTaskFunctions)

# Set the output directory for the library. This is where the library will be
# copied to when the project is built.
set_target_properties(RealTimeTasks_RTTaskFunctions PROPERTIES 
  COMPILE_WARNING_AS_ERROR ON
  OUTPUT_NAME "RTTaskFunctions"
  ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  LIBRARY_OUTPUT_DIRECTORY_RELEASE ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  LIBRARY_OUTPUT_DIRECTORY_DEBUG ${RTTASK_FUNCTIONS_OUTPUT_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${RTTASK_FUNCTIONS_OUTPUT_DIR}
)

# If we're on Windows, then we need create a target for calling the INtime build
if (WINDOWS_BUILD)
  configure_intime_target(RealTimeTasks_RTTaskFunctions)
endif()
