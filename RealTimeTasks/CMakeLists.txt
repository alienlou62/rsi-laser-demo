cmake_minimum_required(VERSION 3.15)

# Enable C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create the project
project(SampleApps_RTTasks)

# The root directory of the C++ sample apps and the cmake helpers
set(SAMPLE_APPS_CPP_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
set(CMAKE_HELPERS_DIR "${SAMPLE_APPS_CPP_ROOT_DIR}/cmake")

# Include the SampleAppsCPP.cmake file, which defines paths to necessary headers
# and libraries as well as a functions to create sample app targets
include("${CMAKE_HELPERS_DIR}/SampleAppsCPP.cmake")

# Add the RTTaskFunctions library
add_subdirectory(RTTaskFunctions)

set(SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

# Helper function to create a sample app target that uses the RTTaskFunctions library
function(create_rt_task_sample_app_target target_name)
  # Use the remaining arguments (ARGN) as source files
  create_sample_app_executable_target(${target_name} ${ARGN})
  add_dependencies(${target_name} RealTimeTasks_RTTaskFunctions)
  target_include_directories(${target_name} PRIVATE ${SOURCE_DIR})
endfunction()

# Create the RTTask sample app targets
create_rt_task_sample_app_target(RealTimeTasks_HelloRTTasks ${SOURCE_DIR}/HelloRTTasks.cpp)
create_rt_task_sample_app_target(RealTimeTasks_RandomWalk ${SOURCE_DIR}/RandomWalk.cpp)
