#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#

###############################################################################
# Minimum CMake version and policies
###############################################################################
cmake_minimum_required(VERSION 3.5.0)
if (POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

if (POLICY CMP0042)
  # Enable `MACOSX_RPATH` by default.
  cmake_policy(SET CMP0042 NEW)
endif()

if (POLICY CMP0037)
  # Disallow reserved target names
  cmake_policy(SET CMP0037 NEW)
endif()

# This overrides the default flags for the different CMAKE_BUILD_TYPEs
set(CMAKE_USER_MAKE_RULES_OVERRIDE_C
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/c_flags_override.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cxx_flags_override.cmake")
project(KLEE CXX C)

include(CheckFunctionExists)
include(CheckLibraryExists)

###############################################################################
# Project version
###############################################################################
set(KLEE_VERSION_MAJOR 2)
set(KLEE_VERSION_MINOR 2-pre)
set(KLEE_VERSION "${KLEE_VERSION_MAJOR}.${KLEE_VERSION_MINOR}")

# If a patch is needed, we can add KLEE_VERSION_PATCH
# set(KLEE_VERSION_PATCH 0)
# set(KLEE_VERSION "${KLEE_VERSION_MAJOR}.${KLEE_VERSION_MINOR}.${KLEE_VERSION_PATCH}")

message(STATUS "KLEE version ${KLEE_VERSION}")
set(PACKAGE_STRING "\"KLEE ${KLEE_VERSION}\"")
set(PACKAGE_URL "\"https://klee.github.io\"")

################################################################################
# Set various useful variables depending on CMake version
################################################################################
set(ADD_CUSTOM_COMMAND_USES_TERMINAL_ARG "USES_TERMINAL")

set(EXTERNAL_PROJECT_ADD_STEP_USES_TERMINAL_ARG "USES_TERMINAL" "1")

################################################################################
# Sanity check - Disallow building in source.
# Otherwise we would overwrite the Makefiles of the old build system.
################################################################################
if ("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
  message(FATAL_ERROR "In source builds are not allowed. You should invoke "
          "CMake from a different directory.")
endif()

################################################################################
# Sanity Check: Check for in source build of the old build system.
# Some build files from the old build system could interfere with our build.
################################################################################
set(KLEE_OLD_BUILD_SYSTEM_FILES
  "include/klee/Config/config.h"
  "test/lit.site.cfg"
)
foreach (legacy_file ${KLEE_OLD_BUILD_SYSTEM_FILES})
  if (EXISTS "${PROJECT_SOURCE_DIR}/${legacy_file}")
    if (EXISTS "${PROJECT_SOURCE_DIR}/.git")
      set(CLEAN_SRC_DIR_INSTRUCTIONS
        "The KLEE source tree apears to be a git repository so you can run"
        " \"git clean -dxn\" to see what files aren't part of the repo and then"
        " run \"git clean -fdx\" to remove them."
      )
    else()
      # This is the only reliable way to fix this.
      set(CLEAN_SRC_DIR_INSTRUCTIONS
        "The KLEE source tree doesn't appear to be a git repository so you will"
        " need to download a fresh copy of KLEE's source code."
      )
    endif()
    message(FATAL_ERROR "\"${PROJECT_SOURCE_DIR}/${legacy_file}\""
      " exists in KLEE's source tree. It is likely that the Autoconf/Makefile"
      " build system was configured to do an in-source build in KLEE's source"
      " tree. This could cause problems with the CMake build. "
      ${CLEAN_SRC_DIR_INSTRUCTIONS}
      " You can then run cmake again."
    )
  endif()
endforeach()

################################################################################
# Build type
################################################################################
message(STATUS "CMake generator: ${CMAKE_GENERATOR}")
if (DEFINED CMAKE_CONFIGURATION_TYPES)
  # Multi-configuration build (e.g. Xcode). Here
  # CMAKE_BUILD_TYPE doesn't matter
  message(STATUS "Available configurations: ${CMAKE_CONFIGURATION_TYPES}")
else()
  # Single configuration generator (e.g. Unix Makefiles, Ninja)
  set(available_build_types Debug Release RelWithDebInfo MinSizeRel)
  if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE is not set. Setting default")
    message(STATUS "The available build types are: ${available_build_types}")
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE String
        "Options are ${available_build_types}"
        FORCE)
    # Provide drop down menu options in cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${available_build_types})
  endif()
  message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

  # Check the selected build type is valid
  list(FIND available_build_types "${CMAKE_BUILD_TYPE}" _build_type_index)
  if ("${_build_type_index}" EQUAL "-1")
    message(FATAL_ERROR "\"${CMAKE_BUILD_TYPE}\" is an invalid build type.\n"
      "Use one of the following build types ${available_build_types}")
  endif()
endif()


# Reference specific library paths used during linking for install
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

################################################################################
# Add our CMake module directory to the list of module search directories
################################################################################
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")

################################################################################
# Compiler flags for KLEE components
# Subsequent commands will append to these. These are used instead of
# directly modifying CMAKE_CXX_FLAGS so that other code can be easily built with
# different flags.
################################################################################
set(KLEE_COMPONENT_EXTRA_INCLUDE_DIRS "")
set(KLEE_COMPONENT_CXX_DEFINES "")
set(KLEE_COMPONENT_CXX_FLAGS "")
set(KLEE_SOLVER_LIBRARIES "")
set(KLEE_COMPONENT_EXTRA_LIBRARIES "")


################################################################################
# Assertions
################################################################################
option(ENABLE_KLEE_ASSERTS "Enable KLEE assertions" ON)
if (ENABLE_KLEE_ASSERTS)
  message(STATUS "KLEE assertions enabled")
  # Assume that -DNDEBUG isn't set.
else()
  message(STATUS "KLEE assertions disabled")
  list(APPEND KLEE_COMPONENT_CXX_DEFINES "-DNDEBUG")
endif()

################################################################################
# KLEE timestamps
################################################################################
option(KLEE_ENABLE_TIMESTAMP "Add timestamps to KLEE sources" OFF)

################################################################################
# Include useful CMake functions
################################################################################
include(GNUInstallDirs)
include(CheckCXXSymbolExists)
include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckPrototypeDefinition)
include(CMakePushCheckState)
include("${PROJECT_SOURCE_DIR}/cmake/string_to_list.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/klee_component_add_cxx_flag.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/add_global_flag.cmake")


################################################################################
# Find LLVM
################################################################################
include(${PROJECT_SOURCE_DIR}/cmake/find_llvm.cmake)
set(NEEDED_LLVM_VARS
  LLVM_PACKAGE_VERSION
  LLVM_VERSION_MAJOR
  LLVM_VERSION_MINOR
  LLVM_VERSION_PATCH
  LLVM_DEFINITIONS
  LLVM_ENABLE_ASSERTIONS
  LLVM_ENABLE_EH
  LLVM_ENABLE_RTTI
  LLVM_INCLUDE_DIRS
  LLVM_LIBRARY_DIRS
  LLVM_TOOLS_BINARY_DIR
  LLVM_ENABLE_VISIBILITY_INLINES_HIDDEN
  TARGET_TRIPLE
)

foreach (vname ${NEEDED_LLVM_VARS})
  message(STATUS "${vname}: \"${${vname}}\"")
  if (NOT (DEFINED "${vname}"))
    message(FATAL_ERROR "${vname} was not defined")
  endif()
endforeach()

set(OPTIONAL_LLVM_VARS
  LLVM_BUILD_MAIN_SRC_DIR
)
foreach (vname ${OPTIONAL_LLVM_VARS})
  if (${vname})
    message(STATUS "${vname}: \"${${vname}}\"")
  endif()
endforeach()

if (LLVM_ENABLE_ASSERTIONS)
  # Certain LLVM debugging macros only work when LLVM was built with asserts
  set(ENABLE_KLEE_DEBUG 1) # for config.h
else()
  unset(ENABLE_KLEE_DEBUG) # for config.h
endif()

# Warn about mixing build types.
# This is likely a bad idea because some of LLVM's header files use the NDEBUG
# macro which can change things like data layout.
if (LLVM_ENABLE_ASSERTIONS AND (NOT ENABLE_KLEE_ASSERTS))
  message(WARNING
    "LLVM was built with assertions but KLEE will be built without them.\n"
    "This might lead to unexpected behaviour."
  )
elseif ((NOT LLVM_ENABLE_ASSERTIONS) AND ENABLE_KLEE_ASSERTS)
  message(WARNING
    "LLVM was built without assertions but KLEE will be built with them.\n"
    "This might lead to unexpected behaviour."
  )
endif()

if (LLVM_ENABLE_VISIBILITY_INLINES_HIDDEN)
  list(APPEND KLEE_COMPONENT_CXX_FLAGS "-fvisibility-inlines-hidden")
endif()


list(APPEND KLEE_COMPONENT_CXX_DEFINES ${LLVM_DEFINITIONS})
list(APPEND KLEE_COMPONENT_EXTRA_INCLUDE_DIRS ${LLVM_INCLUDE_DIRS})

# Find llvm-link
set(LLVM_LINK "${LLVM_TOOLS_BINARY_DIR}/llvm-link")
if (NOT EXISTS "${LLVM_LINK}")
  message(FATAL_ERROR "Failed to find llvm-link at \"${LLVM_LINK}\"")
endif()

# Find llvm-ar
set(LLVM_AR "${LLVM_TOOLS_BINARY_DIR}/llvm-ar")
if (NOT EXISTS "${LLVM_AR}")
  message(FATAL_ERROR "Failed to find llvm-ar at \"${LLVM_AR}\"")
endif()

# Find llvm-as
set(LLVM_AS "${LLVM_TOOLS_BINARY_DIR}/llvm-as")
if (NOT EXISTS "${LLVM_AS}")
  message(FATAL_ERROR "Failed to find llvm-as at \"${LLVM_AS}\"")
endif()

################################################################################
# C++ version
################################################################################
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

################################################################################
# Warnings
################################################################################
include(${PROJECT_SOURCE_DIR}/cmake/compiler_warnings.cmake)

################################################################################
# Solvers
################################################################################
set(ENABLE_Z3 ON)
include(${PROJECT_SOURCE_DIR}/cmake/find_z3.cmake)

###############################################################################
# Exception handling
###############################################################################
if (NOT LLVM_ENABLE_EH)
  if (ENABLE_SOLVER_METASMT)
    message(WARNING "Not disabling exceptions because metaSMT uses them")
  else()
    klee_component_add_cxx_flag("-fno-exceptions" REQUIRED)
  endif()
endif()

###############################################################################
# RTTI
###############################################################################
if (NOT LLVM_ENABLE_RTTI)
  if (ENABLE_SOLVER_METASMT AND metaSMT_REQUIRE_RTTI)
    message(FATAL_ERROR
      "RTTI cannot be disabled because metaSMT uses them."
      "This build configuration is not supported and will likely not work."
      "You should recompile LLVM with RTTI enabled.")
  else()
    klee_component_add_cxx_flag("-fno-rtti" REQUIRED)
  endif()
endif()

################################################################################
# Detect libcap
################################################################################
check_include_file("sys/capability.h" HAVE_SYS_CAPABILITY_H)
if (HAVE_SYS_CAPABILITY_H)
  find_library(LIBCAP_LIBRARIES
    NAMES cap
    DOC "libcap library"
  )
# On FreeBSD <sys/capability.h> is a different thing
  if (NOT LIBCAP_LIBRARIES OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(HAVE_SYS_CAPABILITY_H 0)
  endif()
else()
  set(LIBCAP_LIBRARIES "")
endif()

################################################################################
# Detect libutil
################################################################################
check_include_file(pty.h HAVE_PTY_H)
check_include_file(util.h HAVE_UTIL_H)
check_include_file(libutil.h HAVE_LIBUTIL_H)
if (HAVE_PTY_H OR HAVE_UTIL_H OR HAVE_LIBUTIL_H)
  check_function_exists(openpty openpty_in_libc)
  if (NOT openpty_in_libc)
    check_library_exists(util openpty "" openpty_in_libutil)
    if (openpty_in_libutil)
      set(LIBUTIL_LIBRARIES util)
    else ()
      message(FATAL_ERROR "Could not find libutil")
    endif (openpty_in_libutil)
  endif (NOT openpty_in_libc)
endif (HAVE_PTY_H OR HAVE_UTIL_H OR HAVE_LIBUTIL_H)

################################################################################
# Miscellaneous header file detection
################################################################################
check_cxx_symbol_exists(__ctype_b_loc ctype.h HAVE_CTYPE_EXTERNALS)
check_cxx_symbol_exists(mallinfo malloc.h HAVE_MALLINFO)
check_cxx_symbol_exists(malloc_zone_statistics malloc/malloc.h HAVE_MALLOC_ZONE_STATISTICS)

check_include_file(sys/statfs.h HAVE_SYSSTATFS_H)

# FIXME: This is needed by the runtime not KLEE itself so we are testing the wrong
# compiler.
check_include_file("selinux/selinux.h" HAVE_SELINUX_SELINUX_H)
check_include_file("sys/acl.h" HAVE_SYS_ACL_H)
if (HAVE_SELINUX_SELINUX_H)
  message(STATUS "SELinux support enabled")
  set(HAVE_SELINUX 1)
  # Test what function signature we need to use for SELinux. The signatures
  # have changed between 2.2 and 2.3. In particular, the type of the "security
  # context" parameter was changed from char * to const char *, with this
  # patch: [PATCH] Get rid of security_context_t and fix const declarations.
  # [http://www.spinics.net/lists/selinux/msg14827.html]
  check_prototype_definition(setcon
    "int setcon(char* context)"
    "0"
    "selinux/selinux.h"
    SELINUX_SECURITY_CTX_NO_CONST
  )
  if (SELINUX_SECURITY_CTX_NO_CONST)
    message(STATUS "security_context_t is char*")
    set(KLEE_SELINUX_CTX_CONST " ")
  else()
    check_prototype_definition(setcon
      "int setcon(const char* context)"
      "0"
      "selinux/selinux.h"
      SELINUX_SECURITY_CTX_WITH_CONST
    )
    if (SELINUX_SECURITY_CTX_WITH_CONST)
      message(STATUS "security_context_t is const char*")
      set(KLEE_SELINUX_CTX_CONST "const")
    else()
      message(FATAL_ERROR "Failed to determine function signature for \"setcon\"")
    endif()
  endif()
else()
  message(STATUS "SELinux support disabled")
  set(HAVE_SELINUX 0)
endif()

################################################################################
# Workarounds
################################################################################
include(${PROJECT_SOURCE_DIR}/cmake/workaround_llvm_pr39177.cmake)

################################################################################
# Global clean target
################################################################################
# CMake already uses the "clean" target name but it doesn't clean everything
# unfortunately. We can't modify the target so we provide our own "clean_all"
# target that runs clean. Other rules for performing clean up should declare
# that "clean_all" depends on those rules.
add_custom_target(clean_all
  # Invoke CMake's own clean target
  COMMAND
    "${CMAKE_COMMAND}"
    "--build"
    "${PROJECT_BINARY_DIR}"
    "--target"
    "clean"
)

################################################################################
# KLEE uclibc support
################################################################################
option(ENABLE_KLEE_UCLIBC "Enable support for klee-uclibc" OFF)
if (ENABLE_KLEE_UCLIBC)
  message(STATUS "klee-uclibc support enabled")
  set(SUPPORT_KLEE_UCLIBC 1) # For config.h
  set(KLEE_UCLIBC_PATH "" CACHE PATH "Path to klee-uclibc root directory")
  if (NOT IS_DIRECTORY "${KLEE_UCLIBC_PATH}")
    message(FATAL_ERROR
      "KLEE_UCLIBC_PATH (\"${KLEE_UCLIBC_PATH}\") is not a valid directory.\n"
      "Try passing -DKLEE_UCLIBC_PATH=<path> to cmake where <path> is the path"
      " to the root of the klee-uclibc directory.")
  endif()

  # Find the C library bitcode archive
  set(KLEE_UCLIBC_BCA_NAME "klee-uclibc.bca")
  set(KLEE_UCLIBC_C_BCA "${KLEE_UCLIBC_PATH}/lib/libc.a")
  if (NOT EXISTS "${KLEE_UCLIBC_C_BCA}")
    message(FATAL_ERROR
      "klee-uclibc library not found at \"${KLEE_UCLIBC_C_BCA}\"")
  endif()
  message(STATUS "Found klee-uclibc library: \"${KLEE_UCLIBC_C_BCA}\"")

  # Make a symlink to KLEE_UCLIBC_C_BCA so KLEE can find it where it
  # is expected.
  file(MAKE_DIRECTORY "${KLEE_RUNTIME_DIRECTORY}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${KLEE_UCLIBC_C_BCA}"
    "${KLEE_RUNTIME_DIRECTORY}/${KLEE_UCLIBC_BCA_NAME}"
  )
  list(APPEND KLEE_COMPONENT_CXX_DEFINES
    -DKLEE_UCLIBC_BCA_NAME=\"${KLEE_UCLIBC_BCA_NAME}\")

  # Add klee-uclibc to the install target. We install the original
  # file rather than the symlink because CMake would just copy the symlink
  # rather than the file.
  install(FILES "${KLEE_UCLIBC_C_BCA}"
    DESTINATION "${KLEE_INSTALL_RUNTIME_DIR}"
    RENAME "${KLEE_UCLIBC_BCA_NAME}"
    )

else()
  message(STATUS "klee-uclibc support disabled")
  set(SUPPORT_KLEE_UCLIBC 0) # For config.h
endif()

################################################################################
# KLEE libcxx support
################################################################################
option(ENABLE_KLEE_LIBCXX "Enable libcxx for klee" OFF)
if (ENABLE_KLEE_LIBCXX)
  message(STATUS "klee-libcxx support enabled")
  set(SUPPORT_KLEE_LIBCXX 1) # For config.h
  set(KLEE_LIBCXX_DIR "" CACHE PATH "Path to root directory with libcxx shared object")
  if (NOT EXISTS "${KLEE_LIBCXX_DIR}")
    message(FATAL_ERROR
      "${KLEE_LIBCXX_PATH} does not exist. Try passing -DKLEE_LIBCXX_DIR=<path>")
  endif()

  if (NOT IS_DIRECTORY "${KLEE_LIBCXX_INCLUDE_DIR}")
    message(FATAL_ERROR
      "${KLEE_LIBCXX_INCLUDE_DIR} does not exist. Try passing -DKLEE_LIBCXX_INCLUDE_DIR=<path>")
  endif()
  message(STATUS "Use libc++ include path: \"${KLEE_LIBCXX_INCLUDE_DIR}\"")

  # Find the library bitcode archive

  # Check for static first
  set(KLEE_LIBCXX_BC_NAME "libc++.bca")
  set(KLEE_LIBCXX_BC_PATH "${KLEE_LIBCXX_DIR}/lib/${KLEE_LIBCXX_BC_NAME}")
  if (NOT EXISTS "${KLEE_LIBCXX_BC_PATH}")
    # Check for dynamic so lib
    set(KLEE_LIBCXX_BC_NAME "libc++.so.bc")
    set(KLEE_LIBCXX_BC_PATH "${KLEE_LIBCXX_DIR}/lib/${KLEE_LIBCXX_BC_NAME}")
    if (NOT EXISTS "${KLEE_LIBCXX_BC_PATH}")
      set(KLEE_LIBCXX_BC_NAME "libc++.dylib.bc")
      set(KLEE_LIBCXX_BC_PATH "${KLEE_LIBCXX_DIR}/lib/${KLEE_LIBCXX_BC_NAME}")
      if (NOT EXISTS "${KLEE_LIBCXX_BC_PATH}")
        message(FATAL_ERROR
          "libc++ library not found at \"${KLEE_LIBCXX_DIR}\"")
      endif()
    endif()
  endif()
  message(STATUS "Found libc++ library: \"${KLEE_LIBCXX_BC_PATH}\"")

  # Make a symlink to KLEE_LIBCXX_EXTERNAL_OBJECT so KLEE can find it where it
  # is expected.
  file(MAKE_DIRECTORY "${KLEE_RUNTIME_DIRECTORY}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${KLEE_LIBCXX_BC_PATH}"
    "${KLEE_RUNTIME_DIRECTORY}/${KLEE_LIBCXX_BC_NAME}"
  )
  list(APPEND KLEE_COMPONENT_CXX_DEFINES
    -DKLEE_LIBCXX_BC_NAME=\"${KLEE_LIBCXX_BC_NAME}\")

  # Add libcxx to the install target. We install the original
  # file rather than the symlink because CMake would just copy the symlink
  # rather than the file.
  install(FILES "${KLEE_LIBCXX_BC_PATH}"
    DESTINATION "${KLEE_INSTALL_RUNTIME_DIR}"
    RENAME "${KLEE_LIBCXX_BC_NAME}"
    )

else()
  message(STATUS "libc++ support disabled")
  set(SUPPORT_KLEE_LIBCXX 0) # For config.h
endif()

################################################################################
# Sanitizer support
################################################################################
message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
set(IS_ASAN_BUILD 0)
set(IS_UBSAN_BUILD 0)
set(IS_MSAN_BUILD 0)
string(REPLACE " " ";" _flags ${CMAKE_CXX_FLAGS})
foreach(arg IN ITEMS ${_flags})
  if (${arg} STREQUAL -fsanitize=address)
    set(IS_ASAN_BUILD 1)
  elseif (${arg} STREQUAL -fsanitize=undefined)
    set(IS_UBSAN_BUILD 1)
  elseif (${arg} STREQUAL -fsanitize=memory)
    set(IS_MSAN_BUILD 1)
  endif()
endforeach()
unset(_flags)

################################################################################
# Generate `config.h`
################################################################################
configure_file(${PROJECT_SOURCE_DIR}/include/klee/Config/config.h.cmin
  ${PROJECT_BINARY_DIR}/include/klee/Config/config.h)

################################################################################
# Generate `CompileTimeInfo.h`
################################################################################
if (EXISTS "${PROJECT_SOURCE_DIR}/.git")
  # Get information from git. We use third-party code to do this. The nice
  # thing about this code is it will trigger a re-configure if the HEAD changes
  # which means when we build KLEE, it should always have the correct git
  # information.
  include(${PROJECT_SOURCE_DIR}/cmake/GetGitRevisionDescription.cmake)
  get_git_head_revision(_NOT_USED_KLEE_GIT_REFSPEC KLEE_GIT_SHA1HASH)
  message(STATUS "KLEE_GIT_SHA1HASH: ${KLEE_GIT_SHA1HASH}")
else()
  set(KLEE_GIT_SHA1HASH "unknown")
endif()
set(AUTO_GEN_MSG "AUTOMATICALLY GENERATED. DO NOT EDIT!")
configure_file(${PROJECT_SOURCE_DIR}/include/klee/Config/CompileTimeInfo.h.cmin
  ${PROJECT_BINARY_DIR}/include/klee/Config/CompileTimeInfo.h
)

################################################################################
# Global include directories
################################################################################
include_directories("${PROJECT_BINARY_DIR}/include")
include_directories("${PROJECT_SOURCE_DIR}/include")

################################################################################
# Set default location for targets in the build directory
################################################################################
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(KLEE_UTILS_DIR ${PROJECT_SOURCE_DIR}/utils)


################################################################################
# Report the value of various variables to aid debugging
################################################################################
message(STATUS "KLEE_COMPONENT_EXTRA_INCLUDE_DIRS: '${KLEE_COMPONENT_EXTRA_INCLUDE_DIRS}'")
message(STATUS "KLEE_COMPONENT_CXX_DEFINES: '${KLEE_COMPONENT_CXX_DEFINES}'")
message(STATUS "KLEE_COMPONENT_CXX_FLAGS: '${KLEE_COMPONENT_CXX_FLAGS}'")
message(STATUS "KLEE_COMPONENT_EXTRA_LIBRARIES: '${KLEE_COMPONENT_EXTRA_LIBRARIES}'")
message(STATUS "KLEE_SOLVER_LIBRARIES: '${KLEE_SOLVER_LIBRARIES}'")

################################################################################
# KLEE components
################################################################################
include("${PROJECT_SOURCE_DIR}/cmake/klee_add_component.cmake")
add_subdirectory(lib)

################################################################################
# Miscellaneous install
################################################################################
install(FILES include/klee/klee.h DESTINATION include/klee)
