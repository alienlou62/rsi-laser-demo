# Include guard
if (INTIME_CMAKE_INCLUDED)
  return()
endif()
set(INTIME_CMAKE_INCLUDED TRUE)

if (DEFINED WIN32 AND NOT DEFINED __INTIME__) # Windows build
  # Run CMake again with the INtime flag set targeting Win32
  set(INTIME_BUILD_DIR "${CMAKE_BINARY_DIR}/INtime")

  function(configure_cmake_for_intime source_dir)
    # Check if the INtime cache is missing.
    if(NOT EXISTS "${INTIME_BUILD_DIR}/CMakeCache.txt")
      message(STATUS "INtime cache not found. Running initial Win32 configuration.")

      # Clean the INtime build folder first
      file(REMOVE_RECURSE "${INTIME_BUILD_DIR}")

      # Configure for Win32 configuration
      execute_process(
        COMMAND "${CMAKE_COMMAND}"
                -B "${INTIME_BUILD_DIR}"
                -S "${source_dir}"
                -G "${CMAKE_GENERATOR}"
                -D "CMAKE_CONFIGURATION_TYPES=Release;Debug"
                -A "Win32"
                -D "__INTIME__=True"
                -D "RMP_DIR=${RMP_DIR}"
        RESULT_VARIABLE result
      )
      if(NOT result EQUAL 0)
        message(FATAL_ERROR "CMake generation failed for Win32 configuration.")
      endif()

      # Remove the stamp list to force re-configuration for INtime.
      file(REMOVE "${INTIME_BUILD_DIR}/CMakeFiles/generate.stamp.list")
    endif()

    # Configure for the INtime configuration on top of the Win32 configuration.
    execute_process(
      COMMAND "${CMAKE_COMMAND}"
              -B "${INTIME_BUILD_DIR}"
              -S "${source_dir}"
              -G "${CMAKE_GENERATOR}"
              -D "CMAKE_CONFIGURATION_TYPES=Release;Debug"
              -D "CMAKE_GENERATOR_PLATFORM=INtime"
              -D "__INTIME__=True"
              -D "RMP_DIR=${RMP_DIR}"
      RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
      message(FATAL_ERROR "CMake generation failed for INtime configuration.")
    endif()

    message(STATUS "Configured INtime build in ${INTIME_BUILD_DIR}")
  endfunction()

  # Add a function to configure a target for INtime by calling the INtime build
  function(configure_intime_target target)
    # Add a custom target to build the target in the INtime configuration
    add_custom_target(${target}_intime
      COMMAND ${CMAKE_COMMAND} --build "${INTIME_BUILD_DIR}" --target ${target} --config $<CONFIG>
    )

    # This dependency makes sure that the INtime build is run before the target
    add_dependencies(${target} ${target}_intime)
  endfunction()

elseif(DEFINED __INTIME__) # INtime build
  # Find the INtime installation directory, if it wasn't set explicitly
  if (NOT DEFINED INTIME_DIRECTORY)
    set(INTIME_DIRECTORY "$ENV{INTIME}")
    file(TO_CMAKE_PATH "${INTIME_DIRECTORY}" INTIME_DIRECTORY)
  endif()

  # Set INtime extensions
  set(CMAKE_EXECUTABLE_SUFFIX ".rta")
  set(CMAKE_SHARED_LIBRARY_SUFFIX ".rsl")

  if (CMAKE_GENERATOR STREQUAL "Visual Studio 17 2022")
    message("Applying INtime configuration for Visual Studio 2022.")
    include(${CMAKE_CURRENT_LIST_DIR}/INtime_VS2022_config.cmake)

  elseif (CMAKE_GENERATOR STREQUAL "Visual Studio 16 2019")
    message("Applying INtime configuration for Visual Studio 2019.")
    include(${CMAKE_CURRENT_LIST_DIR}/INtime_VS2019_config.cmake)

  else()
    message(WARNING "Expected 'Visual Studio 17 2022' or 'Visual Studio 16 2019', 
            but detected '${CMAKE_GENERATOR}'. Defaulting to Visual Studio 2019 
            configuration. There may be incompatibilities.")
    message("Applying INtime configuration for Visual Studio 2019.")
    include(${CMAKE_CURRENT_LIST_DIR}/INtime_VS2019_config.cmake)

  endif()
endif()
