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

# Find gRPC and Protobuf
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)
pkg_check_modules(GRPC_CPP REQUIRED grpc++_reflection)

# Generate protobuf files
set(PROTO_FILES
    ${CMAKE_CURRENT_LIST_DIR}/ui/RapidLaser/Assets/protos/camera_streaming.proto
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Generate gRPC files
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
if(NOT GRPC_CPP_PLUGIN)
    message(FATAL_ERROR "grpc_cpp_plugin not found!")
endif()

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/camera_streaming.grpc.pb.cc
           ${CMAKE_CURRENT_BINARY_DIR}/camera_streaming.grpc.pb.h
    COMMAND protobuf::protoc
    ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
         --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
         -I${CMAKE_CURRENT_LIST_DIR}/ui/RapidLaser/Assets/protos
         ${CMAKE_CURRENT_LIST_DIR}/ui/RapidLaser/Assets/protos/camera_streaming.proto
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/ui/RapidLaser/Assets/protos/camera_streaming.proto
    COMMENT "Generating gRPC files"
)

set(GRPC_SRCS 
    ${CMAKE_CURRENT_BINARY_DIR}/camera_streaming.grpc.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/camera_streaming.grpc.pb.h
)

# Setup the target that doesn't use RTTasks
add_executable(LaserDemo 
  ${CMAKE_CURRENT_LIST_DIR}/src/demo_main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/image_processing.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/motion_control.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/helpers/camera_helpers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/helpers/rmp_helpers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/helpers/misc_helpers.cpp
)
setup_rmp_target(LaserDemo)
target_include_directories(LaserDemo PRIVATE 
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/include/helpers
  ${OpenCV_INCLUDE_DIRS}
  ${PYLON_INCLUDE_DIRS}
)
target_link_libraries(LaserDemo PRIVATE 
  ${OpenCV_LIBRARIES}
  pylon::pylon
)
target_compile_definitions(LaserDemo PRIVATE CONFIG_FILE="${CMAKE_CURRENT_LIST_DIR}/acA640-300gc_22441779.pfs")
target_compile_options(LaserDemo PRIVATE "-Wno-deprecated-enum-enum-conversion")

# Setup the target that uses RTTasks
add_subdirectory(RTTaskFunctions)
add_executable(LaserDemoRTTasks 
  ${CMAKE_CURRENT_LIST_DIR}/src/rttasks_demo_main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/camera_grpc_server.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/helpers/rmp_helpers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/helpers/misc_helpers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/helpers/camera_helpers.cpp
  ${PROTO_SRCS}
  ${GRPC_SRCS}
)
setup_rmp_target(LaserDemoRTTasks)
target_include_directories(LaserDemoRTTasks PRIVATE 
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/include/helpers
  ${CMAKE_CURRENT_BINARY_DIR}  # For generated protobuf files
  ${PYLON_INCLUDE_DIRS}
)
target_link_libraries(LaserDemoRTTasks PRIVATE 
  pylon::pylon
  protobuf::libprotobuf
  ${GRPC_LIBRARIES}
  ${GRPC_CPP_LIBRARIES}
)
target_compile_definitions(LaserDemoRTTasks PRIVATE 
  CONFIG_FILE="${CMAKE_CURRENT_LIST_DIR}/acA640-300gc_22441779.pfs"
)

# Setup the testing (sandbox) target
add_executable(Sandbox
  ${CMAKE_CURRENT_LIST_DIR}/sandbox/sandbox_main.cpp 
  ${CMAKE_CURRENT_LIST_DIR}/sandbox/helpers/image_helpers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/image_processing.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/motion_control.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/helpers/camera_helpers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/helpers/rmp_helpers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/helpers/misc_helpers.cpp
)
setup_rmp_target(Sandbox)
target_include_directories(Sandbox PRIVATE 
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/include/helpers
  ${CMAKE_CURRENT_LIST_DIR}/sandbox
  ${CMAKE_CURRENT_LIST_DIR}/sandbox/helpers
  ${OpenCV_INCLUDE_DIRS}
  ${PYLON_INCLUDE_DIRS}
)
target_link_libraries(Sandbox PRIVATE 
  ${OpenCV_LIBRARIES}
  pylon::pylon
)
target_compile_definitions(Sandbox PRIVATE SANDBOX_DIR="${CMAKE_CURRENT_LIST_DIR}/sandbox/")
target_compile_options(Sandbox PRIVATE "-Wno-deprecated-enum-enum-conversion")