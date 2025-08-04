cmake_minimum_required(VERSION 3.10)
project(LaserDemo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include our RapidSoftware utilities
include(cmake/RapidSoftware.cmake)

find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)

# Use find_package for gRPC system packages (Ubuntu)
find_package(Protobuf REQUIRED)
find_library(GRPC_GRPCPP_LIBRARY grpc++ REQUIRED)
find_library(GRPC_LIBRARY grpc REQUIRED)
find_library(GRPC_GPR_LIBRARY gpr REQUIRED)
find_library(GRPC_ADDRESS_SORTING_LIBRARY address_sorting REQUIRED)

# Set gRPC libraries
set(GRPC_LIBRARIES 
    ${GRPC_GRPCPP_LIBRARY}
    ${GRPC_LIBRARY}  
    ${GRPC_GPR_LIBRARY}
    ${GRPC_ADDRESS_SORTING_LIBRARY}
    ${Protobuf_LIBRARIES}
)

# Add Pylon library directory
link_directories(/opt/pylon/lib)

# Generate protobuf files
set(PROTO_FILES
    ${CMAKE_CURRENT_LIST_DIR}/ui/RapidLaser/Assets/protos/camera_streaming.proto
)

find_program(PROTOBUF_PROTOC_EXECUTABLE protoc REQUIRED)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)

# Generate protobuf files
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/camera_streaming.pb.cc
           ${CMAKE_CURRENT_BINARY_DIR}/camera_streaming.pb.h
    COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
    ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
         -I${CMAKE_CURRENT_LIST_DIR}/ui/RapidLaser/Assets/protos
         ${CMAKE_CURRENT_LIST_DIR}/ui/RapidLaser/Assets/protos/camera_streaming.proto
    DEPENDS ${PROTO_FILES}
    COMMENT "Generating protobuf files"
)

# Generate gRPC files
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/camera_streaming.grpc.pb.cc
           ${CMAKE_CURRENT_BINARY_DIR}/camera_streaming.grpc.pb.h
    COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
    ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
         -I${CMAKE_CURRENT_LIST_DIR}/ui/RapidLaser/Assets/protos
         ${CMAKE_CURRENT_LIST_DIR}/ui/RapidLaser/Assets/protos/camera_streaming.proto
    DEPENDS ${PROTO_FILES}
    COMMENT "Generating gRPC files"
)

# Include directories
include_directories(include)
include_directories(include/helpers)
include_directories(sandbox/helpers)
include_directories(RTTaskFunctions/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(/opt/pylon/include)

# Create a library for the generated protobuf/grpc files
add_library(proto_grpc
    ${CMAKE_CURRENT_BINARY_DIR}/camera_streaming.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/camera_streaming.grpc.pb.cc
)

target_link_libraries(proto_grpc ${GRPC_LIBRARIES})

# Primary executable
add_executable(LaserDemo
    src/demo_main.cpp
    src/camera_grpc_server.cpp
    src/image_processing.cpp
    src/motion_control.cpp
    src/helpers/camera_helpers.cpp
    src/helpers/misc_helpers.cpp
    src/helpers/rmp_helpers.cpp
)

target_link_libraries(LaserDemo
    proto_grpc
    ${GRPC_LIBRARIES}
    ${OpenCV_LIBS}
    pylonbase
    GenApi_gcc_v3_1_Basler_pylon_v3
    GCBase_gcc_v3_1_Basler_pylon_v3
    Log_gcc_v3_1_Basler_pylon_v3
    MathParser_gcc_v3_1_Basler_pylon_v3
    NodeMapData_gcc_v3_1_Basler_pylon_v3
    pthread
)

# RTTask version
add_executable(LaserDemoRTTasks
    src/rttasks_demo_main.cpp
    src/camera_grpc_server.cpp
    src/image_processing.cpp
    src/motion_control.cpp
    src/helpers/camera_helpers.cpp
    src/helpers/misc_helpers.cpp
    src/helpers/rmp_helpers.cpp
)

target_link_libraries(LaserDemoRTTasks
    proto_grpc
    ${GRPC_LIBRARIES}
    ${OpenCV_LIBS}
    pylonbase
    GenApi_gcc_v3_1_Basler_pylon_v3
    GCBase_gcc_v3_1_Basler_pylon_v3
    Log_gcc_v3_1_Basler_pylon_v3
    MathParser_gcc_v3_1_Basler_pylon_v3
    NodeMapData_gcc_v3_1_Basler_pylon_v3
    pthread
    RTTaskFunctions
)

# Sandbox executable (disabled temporarily due to header issues)
# add_executable(Sandbox
#     sandbox/sandbox_main.cpp
#     sandbox/helpers/image_helpers.cpp
# )

# target_link_libraries(Sandbox
#     ${OpenCV_LIBS}
#     pylonbase
#     GenApi_gcc_v3_1_Basler_pylon_v3
#     GCBase_gcc_v3_1_Basler_pylon_v3
#     Log_gcc_v3_1_Basler_pylon_v3
#     MathParser_gcc_v3_1_Basler_pylon_v3
#     NodeMapData_gcc_v3_1_Basler_pylon_v3
#     pthread
# )

# Add subdirectory for RTTaskFunctions (this will be conditional)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RTTaskFunctions")
    add_subdirectory(RTTaskFunctions)
endif()
