# INtime_VS2019_config.cmake
# Include guard
if (INTIME_VS2019_CMAKE_INCLUDED)
  return()
endif()
set(INTIME_VS2019_CMAKE_INCLUDED TRUE)

# Make sure the compiler is MSVC
if (NOT MSVC)
  message(FATAL_ERROR "INtime_VS2019_config.cmake is only for MSVC")
endif()

## C/C++ Configuration

# Set the INtime include directories (must be in this order)
include_directories(
  ${INTIME_DIRECTORY}/rt/include/cpp20
  ${INTIME_DIRECTORY}/rt/include/network7
  ${INTIME_DIRECTORY}/rt/include
)

# Add the INtime preprocessor definitions
add_compile_definitions(
  __C99__
  __INTIME__
  __INTIME_CPP20
  _HAS_NAMESPACE
  _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
  _SILENCE_BOGUS_WARNINGS
  _HAS_FEATURES_REMOVED_IN_CXX20
  UNICODE
  _UNICODE
  # Only in Debug builds:
  $<$<CONFIG:Debug>:__BYPASS_ITERATOR_DEBUG_LEVEL_2__>
)

# Ignore standard include files
add_compile_options(/X)

# Set security check based on configuration
add_compile_definitions(
  $<$<CONFIG:Debug>:/GS>
  $<$<CONFIG:Release>:/GS->
)

# Compile as C++ code
add_compile_options(/TP)

# Use legacy mode to allow Microsoft-specific non-standard extensions
add_compile_options(/permissive)

# Remove RTC flags for Debug builds
string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
string(REPLACE "/RTCs" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
string(REPLACE "/RTCu" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")


## Linker Configuration

# Disable incremental linking
add_link_options(/INCREMENTAL:NO)

# Add additional library directories
link_directories(${INTIME_DIRECTORY}/rt/lib)

# Add additional libraries
link_libraries(
  vshelper17
  clib
  rt
  pcibus
  netlib
  # C++ runtime libraries for Debug/Release
  rtpp17$<$<CONFIG:Debug>:d>
  cpplib17$<$<CONFIG:Debug>:d>
)

# Ignore default libraries
add_link_options(/NODEFAULTLIB)

# Subsystem configuration
add_link_options(/SUBSYSTEM:CONSOLE)

# Configure stack size settings
set(STACK_RESERVE_SIZE "100000000")
set(STACK_COMMIT_SIZE  "1000000")
