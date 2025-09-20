include(cmake/SystemLink.cmake)
include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


include(CheckCXXSourceCompiles)

function(jaxie_propagate_windows_asan_runtime target)
  if(NOT MSVC)
    return()
  endif()
  if(NOT Jaxie_ENABLE_SANITIZER_ADDRESS)
    return()
  endif()
  if(NOT DEFINED JAXIE_ASAN_RUNTIME_DLL OR NOT JAXIE_ASAN_RUNTIME_DLL)
    message(WARNING "AddressSanitizer runtime is not available; target ${target} may fail to launch.")
    return()
  endif()

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${JAXIE_ASAN_RUNTIME_DLL}"
            $<TARGET_FILE_DIR:${target}>)
endfunction()


macro(Jaxie_supports_sanitizers)
  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)

    message(STATUS "Sanity checking UndefinedBehaviorSanitizer, it should be supported on this platform")
    set(TEST_PROGRAM "int main() { return 0; }")

    # Check if UndefinedBehaviorSanitizer works at link time
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=undefined")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=undefined")
    check_cxx_source_compiles("${TEST_PROGRAM}" HAS_UBSAN_LINK_SUPPORT)

    if(HAS_UBSAN_LINK_SUPPORT)
      message(STATUS "UndefinedBehaviorSanitizer is supported at both compile and link time.")
      set(SUPPORTS_UBSAN ON)
    else()
      message(WARNING "UndefinedBehaviorSanitizer is NOT supported at link time.")
      set(SUPPORTS_UBSAN OFF)
    endif()
  else()
    set(SUPPORTS_UBSAN OFF)
  endif()

  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND WIN32)
    set(SUPPORTS_ASAN OFF)
  else()
    if (NOT WIN32)
      message(STATUS "Sanity checking AddressSanitizer, it should be supported on this platform")
      set(TEST_PROGRAM "int main() { return 0; }")

      # Check if AddressSanitizer works at link time
      set(CMAKE_REQUIRED_FLAGS "-fsanitize=address")
      set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address")
      check_cxx_source_compiles("${TEST_PROGRAM}" HAS_ASAN_LINK_SUPPORT)

      if(HAS_ASAN_LINK_SUPPORT)
        message(STATUS "AddressSanitizer is supported at both compile and link time.")
        set(SUPPORTS_ASAN ON)
      else()
        message(WARNING "AddressSanitizer is NOT supported at link time.")
        set(SUPPORTS_ASAN OFF)
      endif()
    else()
      set(SUPPORTS_ASAN ON)
    endif()
  endif()
endmacro()

macro(Jaxie_setup_options)
  option(Jaxie_ENABLE_HARDENING "Enable hardening" ON)
  option(Jaxie_ENABLE_COVERAGE "Enable coverage reporting" OFF)
  cmake_dependent_option(
    Jaxie_ENABLE_GLOBAL_HARDENING
    "Attempt to push hardening options to built dependencies"
    ON
    Jaxie_ENABLE_HARDENING
    OFF)

  Jaxie_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR Jaxie_PACKAGING_MAINTAINER_MODE)
    option(Jaxie_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(Jaxie_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(Jaxie_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(Jaxie_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(Jaxie_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(Jaxie_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(Jaxie_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(Jaxie_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(Jaxie_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(Jaxie_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(Jaxie_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(Jaxie_ENABLE_PCH "Enable precompiled headers" OFF)
    option(Jaxie_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(Jaxie_ENABLE_IPO "Enable IPO/LTO" ON)
    option(Jaxie_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(Jaxie_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(Jaxie_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(Jaxie_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(Jaxie_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(Jaxie_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(Jaxie_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(Jaxie_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(Jaxie_ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
    option(Jaxie_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
    option(Jaxie_ENABLE_PCH "Enable precompiled headers" OFF)
    option(Jaxie_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      Jaxie_ENABLE_IPO
      Jaxie_WARNINGS_AS_ERRORS
      Jaxie_ENABLE_USER_LINKER
      Jaxie_ENABLE_SANITIZER_ADDRESS
      Jaxie_ENABLE_SANITIZER_LEAK
      Jaxie_ENABLE_SANITIZER_UNDEFINED
      Jaxie_ENABLE_SANITIZER_THREAD
      Jaxie_ENABLE_SANITIZER_MEMORY
      Jaxie_ENABLE_UNITY_BUILD
      Jaxie_ENABLE_CLANG_TIDY
      Jaxie_ENABLE_CPPCHECK
      Jaxie_ENABLE_COVERAGE
      Jaxie_ENABLE_PCH
      Jaxie_ENABLE_CACHE)
  endif()

  Jaxie_check_libfuzzer_support(LIBFUZZER_SUPPORTED)
  if(LIBFUZZER_SUPPORTED AND (Jaxie_ENABLE_SANITIZER_ADDRESS OR Jaxie_ENABLE_SANITIZER_THREAD OR Jaxie_ENABLE_SANITIZER_UNDEFINED))
    set(DEFAULT_FUZZER ON)
  else()
    set(DEFAULT_FUZZER OFF)
  endif()

  option(Jaxie_BUILD_FUZZ_TESTS "Enable fuzz testing executable" ${DEFAULT_FUZZER})

endmacro()

macro(Jaxie_global_options)
  if(Jaxie_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    Jaxie_enable_ipo()
  endif()

  Jaxie_supports_sanitizers()

  if(Jaxie_ENABLE_HARDENING AND Jaxie_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR Jaxie_ENABLE_SANITIZER_UNDEFINED
       OR Jaxie_ENABLE_SANITIZER_ADDRESS
       OR Jaxie_ENABLE_SANITIZER_THREAD
       OR Jaxie_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    message("${Jaxie_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${Jaxie_ENABLE_SANITIZER_UNDEFINED}")
    Jaxie_enable_hardening(Jaxie_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()
endmacro()

macro(Jaxie_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(Jaxie_warnings INTERFACE)
  add_library(Jaxie_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  Jaxie_set_project_warnings(
    Jaxie_warnings
    ${Jaxie_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  if(Jaxie_ENABLE_USER_LINKER)
    include(cmake/Linker.cmake)
    Jaxie_configure_linker(Jaxie_options)
  endif()

  include(cmake/Sanitizers.cmake)
  Jaxie_enable_sanitizers(
    Jaxie_options
    ${Jaxie_ENABLE_SANITIZER_ADDRESS}
    ${Jaxie_ENABLE_SANITIZER_LEAK}
    ${Jaxie_ENABLE_SANITIZER_UNDEFINED}
    ${Jaxie_ENABLE_SANITIZER_THREAD}
    ${Jaxie_ENABLE_SANITIZER_MEMORY})

  if(MSVC AND Jaxie_ENABLE_SANITIZER_ADDRESS AND NOT JAXIE_ASAN_RUNTIME_DLL)
    get_filename_component(_jaxie_cl_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(_jaxie_asan_candidates "${_jaxie_cl_dir}/clang_rt.asan_dynamic-x86_64.dll")

    if(DEFINED ENV{VSINSTALLDIR})
      list(APPEND _jaxie_asan_candidates
        "$ENV{VSINSTALLDIR}/VC/Tools/Llvm/x64/bin/clang_rt.asan_dynamic-x86_64.dll")

      file(GLOB _jaxie_llvm_runtime_candidates
           "$ENV{VSINSTALLDIR}/VC/Tools/Llvm/x64/lib/clang/*/lib/windows/clang_rt.asan_dynamic-x86_64.dll")
      list(APPEND _jaxie_asan_candidates ${_jaxie_llvm_runtime_candidates})
    endif()

    foreach(_candidate IN LISTS _jaxie_asan_candidates)
      if(EXISTS "${_candidate}")
        set(JAXIE_ASAN_RUNTIME_DLL "${_candidate}" CACHE INTERNAL "Resolved AddressSanitizer runtime" FORCE)
        break()
      endif()
    endforeach()

    if(NOT JAXIE_ASAN_RUNTIME_DLL)
      message(WARNING "Unable to locate clang_rt.asan_dynamic-x86_64.dll; sanitize-enabled binaries may not run.")
    endif()
  endif()

  # When building with clang-cl + Ninja on Windows, ensure the correct CRT
  # default libraries are pulled in explicitly. This avoids toolchain/env
  # mismatches that can surface as unresolved mainCRTStartup or x86 CRT picks.
  if (MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_link_options(Jaxie_options INTERFACE
      
      #"/defaultlib:vcruntimed"
      #"/defaultlib:ucrtd"
      #"/defaultlib:msvcrtd"
    )
  endif()

  set_target_properties(Jaxie_options PROPERTIES UNITY_BUILD ${Jaxie_ENABLE_UNITY_BUILD})

  if(Jaxie_ENABLE_PCH)
    target_precompile_headers(
      Jaxie_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(Jaxie_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    Jaxie_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(Jaxie_ENABLE_CLANG_TIDY)
    Jaxie_enable_clang_tidy(Jaxie_options ${Jaxie_WARNINGS_AS_ERRORS})
  endif()

  if(Jaxie_ENABLE_CPPCHECK)
    Jaxie_enable_cppcheck(${Jaxie_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(Jaxie_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    Jaxie_enable_coverage(Jaxie_options)
  endif()

  if(Jaxie_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(Jaxie_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  if(Jaxie_ENABLE_HARDENING AND NOT Jaxie_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR Jaxie_ENABLE_SANITIZER_UNDEFINED
       OR Jaxie_ENABLE_SANITIZER_ADDRESS
       OR Jaxie_ENABLE_SANITIZER_THREAD
       OR Jaxie_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    Jaxie_enable_hardening(Jaxie_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()

endmacro()
