# Include guard
if (RAPID_SOFTWARE_CMAKE_INCLUDED)
  return()
endif()
set(RAPID_SOFTWARE_CMAKE_INCLUDED TRUE)

# -----------------------------------------------------------------------------
# RapidSoftware.cmake
#
# Purpose:
#   - Detects host platform (Windows, INtime, or Linux), sets up version
#     variables, and configures import targets for the RapidCode and RTTasks
#     libraries.
#   - Exposes a helper function `setup_rmp_target()` that:
#       • Adds include paths for RapidCode headers
#       • Links against the imported rapidcode library
#       • Adds build-order dependencies on rapidcode and rttasks
#       • Defines a compile-time macro RMP_DEFAULT_PATH
#       • Optionally adds in a DLL‑copy step on Windows/INtime
#
# Optional user‑defined variables (define *before* including this file):
#
#   RMP_DIR
#     – Path to your RapidCode installation directory.
#     – If not set:
#         • On Windows/INtime: defaults to "C:/RSI/${RAPID_SOFTWARE_VERSION}"
#         • On Linux:          defaults to "/rsi"
#
#   RAPID_SOFTWARE_DLLS_OUTPUT_DIR
#     – Directory where RapidSoftware DLLs will be copied (Windows/INtime only).
#     – If set, a custom target “CopyRapidSoftwareDlls” will copy all required DLLs
#       into this folder after build.
# -----------------------------------------------------------------------------

# Version information
set(RAPID_SOFTWARE_VERSION_MAJOR 10)
set(RAPID_SOFTWARE_VERSION_MINOR 6)
set(RAPID_SOFTWARE_VERSION_MICRO 9)
set(RAPID_SOFTWARE_VERSION_PATCH 0)
set(RAPID_SOFTWARE_VERSION 10.6.9 CACHE STRING "Rapid Software version")

# Figure out what platform we're building on.
set(LINUX_BUILD FALSE)
set(INTIME_BUILD FALSE)
set(WINDOWS_BUILD FALSE)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(LINUX_BUILD TRUE)
elseif (DEFINED __INTIME__)
  set(INTIME_BUILD TRUE)
elseif (DEFINED WIN32)
  set(WINDOWS_BUILD TRUE)
else()
  message(FATAL_ERROR "Unsupported platform")
endif()

# RMP_DIR is the directory where the RapidCode executables and libraries are located.
if (DEFINED RMP_DIR)
  # If the RMP_DIR variable is defined, make sure its a CMake path
  file(TO_CMAKE_PATH "${RMP_DIR}" RMP_DIR)
else()
  # Set the default RMP directory based on the platform
  if (WINDOWS_BUILD OR INTIME_BUILD)
    set(RMP_DIR "C:/RSI/${RAPID_SOFTWARE_VERSION}")
  elseif (LINUX_BUILD)
    set(RMP_DIR "/rsi")
  else()
    message(FATAL_ERROR "Unsupported platform")
  endif()
endif()

# Add targets for the RapidCode and RTTasks libraries
add_library(rapidcode SHARED IMPORTED)
add_library(rttasks SHARED IMPORTED)

if (WINDOWS_BUILD OR INTIME_BUILD)
  # Include the CMake helper file for INtime builds
  include("${CMAKE_CURRENT_LIST_DIR}/INtime/INtimeHelper.cmake")

  # The RapidCode libraries to use based on platform
  if (INTIME_BUILD)
    set(RAPIDCODE_IMPLIB ${RMP_DIR}/lib/rt/RapidCodeRT.lib)
    list(APPEND RAPID_SOFTWARE_DLLS
      ${RMP_DIR}/RapidCodeRT.rsl
    )
  elseif (CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
    set(RAPIDCODE_IMPLIB ${RMP_DIR}/lib/x64/RapidCode64.lib)
    list(APPEND RAPID_SOFTWARE_DLLS
      ${RMP_DIR}/RapidCode64.dll
      ${RMP_DIR}/KDL.dll
      ${RMP_DIR}/INtimeLicenseLibrary64.dll
    )
  else()
    set(RAPIDCODE_IMPLIB ${RMP_DIR}/lib/x86/RapidCode.lib)
    list(APPEND RAPID_SOFTWARE_DLLS
      ${RMP_DIR}/x86/RapidCode.dll
      ${RMP_DIR}/x86/KDL.dll
      ${RMP_DIR}/x86/INtimeLicenseLibrary.dll
    )
  endif()

  list(APPEND RAPID_SOFTWARE_DLLS
    ${RMP_DIR}/rttask.dll
  )

  # If the output directory for the DLLs is defined, then add a custom target to copy them
  if (DEFINED RAPID_SOFTWARE_DLLS_OUTPUT_DIR)
    add_custom_target(CopyRapidSoftwareDlls ALL
      DEPENDS ${RAPID_SOFTWARE_DLLS}
      COMMENT "Copying RAPIDSOFTWARE DLLs to ${RAPID_SOFTWARE_DLLS_OUTPUT_DIR}"
      COMMAND ${CMAKE_COMMAND} -E make_directory $<1:${RAPID_SOFTWARE_DLLS_OUTPUT_DIR}>
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RAPID_SOFTWARE_DLLS} $<1:${RAPID_SOFTWARE_DLLS_OUTPUT_DIR}> COMMAND_EXPAND_LISTS
    )
    list(APPEND RAPID_SOFTWARE_DEPENDENCIES CopyRapidSoftwareDlls)
  else()
    message(AUTHOR_WARNING "RAPID_SOFTWARE_DLLS_OUTPUT_DIR not defined. DLLs will not be copied.")
  endif()

  # Setup the RapidCode library target
  set_target_properties(rapidcode PROPERTIES
    IMPORTED_LOCATION ${RMP_DIR}
    IMPORTED_IMPLIB ${RAPIDCODE_IMPLIB}
  )

  # Setup the RTTasks library target
  set_target_properties(rttasks PROPERTIES
    IMPORTED_LOCATION ${RMP_DIR}
  )
elseif (LINUX_BUILD)
  # Setup the RapidCode library target
  set_target_properties(rapidcode PROPERTIES
    IMPORTED_LOCATION ${RMP_DIR}/librapidcode.so
  )

  # Setup the RTTasks library target
  set_target_properties(rttasks PROPERTIES
    IMPORTED_LOCATION ${RMP_DIR}/librttasks.so
  )
endif()

# Function to setup a target that uses RMP
function(setup_rmp_target target)
  target_include_directories(${target} PUBLIC ${RMP_DIR}/include)
  target_link_libraries(${target} PUBLIC rapidcode)
  add_dependencies(${target} rapidcode rttasks)
  target_compile_definitions(${target} PUBLIC RMP_DEFAULT_PATH="${RMP_DIR}")

  # If we're on Windows, and RAPID_SOFTWARE_DLLS_OUTPUT_DIR is defined, then add
  # a dependency on the special target that copies the DLLs to the output directory
  if (WINDOWS_BUILD OR INTIME_BUILD)
    if (TARGET CopyRapidSoftwareDlls)
      add_dependencies(${target} CopyRapidSoftwareDlls)
    endif()
  endif()
endfunction()
