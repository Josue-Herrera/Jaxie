include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(Jaxie_setup_dependencies)

  # For each dependency, see if it's
  # already been provided to us by a parent project

  if(NOT TARGET fmtlib::fmtlib)
    cpmaddpackage("gh:fmtlib/fmt#11.1.4")
  endif()

  if(NOT TARGET spdlog::spdlog)
    cpmaddpackage(
      NAME
      spdlog
      VERSION
      1.15.2
      GITHUB_REPOSITORY
      "gabime/spdlog"
      OPTIONS
      "SPDLOG_FMT_EXTERNAL ON")
  endif()

  if(NOT TARGET Catch2::Catch2WithMain)
    cpmaddpackage("gh:catchorg/Catch2@3.8.1")
  endif()

  if(NOT TARGET CLI11::CLI11)
    cpmaddpackage("gh:CLIUtils/CLI11@2.5.0")
  endif()

  if(NOT TARGET tools::tools)
    cpmaddpackage("gh:lefticus/tools#update_build_system")
  endif()

  # Miniaudio (single-header). Create an interface target when enabled.
  if(JAXIE_USE_MINIAUDIO AND NOT TARGET miniaudio::miniaudio)
    CPMAddPackage(
      NAME miniaudio
      GITHUB_REPOSITORY mackron/miniaudio
      GIT_TAG 0.11.21
      DOWNLOAD_ONLY YES)
    if(miniaudio_ADDED)
      add_library(miniaudio INTERFACE)
      # Expose the fetched directory so consumers can include <miniaudio.h>
      target_include_directories(miniaudio INTERFACE "${miniaudio_SOURCE_DIR}")
      add_library(miniaudio::miniaudio ALIAS miniaudio)
    endif()
  endif()

endfunction()
