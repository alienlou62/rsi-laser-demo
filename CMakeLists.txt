cmake_minimum_required(VERSION 3.10)
project(LaserDemo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include our RapidSoftware utilities
include(cmake/RapidSoftware.cmake)

find_package(OpenCV REQUIRED)

# Add Pylon library directory
link_directories(/opt/pylon/lib)

# Include directories
include_directories(BEFORE include)
include_directories(BEFORE include/helpers)
include_directories(BEFORE sandbox/helpers)
include_directories(BEFORE RTTaskFunctions/src)
include_directories(BEFORE /opt/pylon/include)
include_directories(BEFORE /usr/include)

# RTTask version - simplified without gRPC
add_executable(LaserDemoRTTasks
    src/rttasks_demo_main.cpp
    src/image_processing.cpp
    src/motion_control.cpp
    src/helpers/camera_helpers.cpp
    src/helpers/misc_helpers.cpp
    src/helpers/rmp_helpers.cpp
)

target_link_libraries(LaserDemoRTTasks
    ${OpenCV_LIBS}
    pylonbase
    pylonutility
    GenApi_gcc_v3_1_Basler_pylon_v3
    GCBase_gcc_v3_1_Basler_pylon_v3
    Log_gcc_v3_1_Basler_pylon_v3
    MathParser_gcc_v3_1_Basler_pylon_v3
    NodeMapData_gcc_v3_1_Basler_pylon_v3
    pthread
    RTTaskFunctions
)

# Add subdirectory for RTTaskFunctions
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RTTaskFunctions")
    add_subdirectory(RTTaskFunctions)
endif()
