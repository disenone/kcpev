macro(fix_default_compiler_settings_)
  if (MSVC)
    foreach (flag_var
             CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
             CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      # We prefer more strict warning checking for building Google Test.
      # Replaces /W3 with /W4 in defaults.
      string(REPLACE "/W3" "/W4" ${flag_var} "${${flag_var}}")
    endforeach()
  endif()
endmacro()

# Defines the compiler/linker flags used to build
macro(config_compiler_and_linker)
  # Defines CMAKE_USE_PTHREADS_INIT and CMAKE_THREAD_LIBS_INIT.
  find_package(Threads)
  fix_default_compiler_settings_()

  if (MSVC)
    set(cxx_base_flags "-GS -W4 -wd4251 -wd4275 -nologo -J -Zi -DWIN32")
    if (MSVC_VERSION LESS 1400)  # 1400 is Visual Studio 2005
      # Suppress spurious warnings MSVC 7.1 sometimes issues.
      # Forcing value to bool.
      set(cxx_base_flags "${cxx_base_flags} -wd4800")
      # Copy constructor and assignment operator could not be generated.
      set(cxx_base_flags "${cxx_base_flags} -wd4511 -wd4512")
      # Compatibility warnings not applicable to Google Test.
      # Resolved overload was found by argument-dependent lookup.
      set(cxx_base_flags "${cxx_base_flags} -wd4675")
    endif()
    if (MSVC_VERSION LESS 1500)  # 1500 is Visual Studio 2008
      # Conditional expression is constant.
      # When compiling with /W4, we get several instances of C4127
      # (Conditional expression is constant). In our code, we disable that
      # warning on a case-by-case basis. However, on Visual Studio 2005,
      # the warning fires on std::list. Therefore on that compiler and earlier,
      # we disable the warning project-wide.
      set(cxx_base_flags "${cxx_base_flags} -wd4127")
    endif()
    if (NOT (MSVC_VERSION LESS 1700))  # 1700 is Visual Studio 2012.
      # Suppress "unreachable code" warning on VS 2012 and later.
      # http://stackoverflow.com/questions/3232669 explains the issue.
      set(cxx_base_flags "${cxx_base_flags} -wd4702")
    endif()
#    if (NOT (MSVC_VERSION GREATER 1900))  # 1900 is Visual Studio 2015
#      # BigObj required for tests.
#      set(cxx_base_flags "${cxx_base_flags} -bigobj")
#    endif()

    set(cxx_flags "${cxx_base_flags} -D_UNICODE -DUNICODE -DWIN32 -D_WIN32")
    set(cxx_flags "${cxx_base_flags} -DSTRICT -DWIN32_LEAN_AND_MEAN")
    set(c_flags "${cxx_flags}")
    set(CMAKE_CXX_FLAGS "${cxx_flags}")
    set(CMAKE_C_FLAGS "${c_flags}")

  elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang" OR ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    set(base_flags "-fpic -pthread -O2 -g -fno-strict-aliasing -fwrapv -Wall -Wextra")
    set(base_flags "${base_flags} -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function")
    set(cxx_flags "-std=c++11 ${base_flags}")
    set(c_flags "-std=gnu11 ${base_flags}")
    set(CMAKE_CXX_FLAGS "${cxx_flags}")
    set(CMAKE_C_FLAGS "${c_flags}")
  endif()
endmacro()

# Defines the libraries.
function(cxx_library_with_type name type cxx_flags)
  # type can be either STATIC or SHARED to denote a static or shared library.
  # ARGN refers to additional arguments after 'cxx_flags'.
  add_library(${name} ${type} ${ARGN})
  set_target_properties(${name}
    PROPERTIES
    COMPILE_FLAGS "${cxx_flags}")
  if (CMAKE_USE_PTHREADS_INIT)
    target_link_libraries(${name} ${CMAKE_THREAD_LIBS_INIT})
  endif()
endfunction()

########################################################################
#
# Helper functions for creating build targets.

function(cxx_shared_library name cxx_flags)
  cxx_library_with_type(${name} SHARED "${cxx_flags}" ${ARGN})
endfunction()

function(cxx_library name cxx_flags)
  cxx_library_with_type(${name} "" "${cxx_flags}" ${ARGN})
endfunction()

# cxx_executable_with_flags(name cxx_flags libs srcs...)
#
# creates a named C++ executable that depends on the given libraries and
# is built from the given source files with the given compiler flags.
function(cxx_executable_with_flags name cxx_flags libs)
  add_executable(${name} ${ARGN})
  if (cxx_flags)
    set_target_properties(${name}
      PROPERTIES
      COMPILE_FLAGS "${cxx_flags}")
  endif()
  if (BUILD_SHARED_LIBS)
    set_target_properties(${name}
      PROPERTIES
      COMPILE_DEFINITIONS "GTEST_LINKED_AS_SHARED_LIBRARY=1")
  endif()
  # To support mixing linking in static and dynamic libraries, link each
  # library in with an extra call to target_link_libraries.
  foreach (lib "${libs}")
    target_link_libraries(${name} ${lib})
  endforeach()
endfunction()

# cxx_executable(name dir lib srcs...)
#
# creates a named target that depends on the given libs and is built
# from the given source files.  dir/name.cc is implicitly included in
# the source file list.
function(cxx_executable name dir libs)
  cxx_executable_with_flags(
    ${name} "${cxx_default}" "${libs}" "${dir}/${name}.cc" ${ARGN})
endfunction()

