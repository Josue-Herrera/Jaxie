
project(jaxie)

add_executable(${PROJECT_NAME} main.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)


target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE Jaxie::Jaxie_options
          Jaxie::Jaxie_warnings)

target_link_system_libraries(
  ${PROJECT_NAME}
  PRIVATE
          CLI11::CLI11
          fmt::fmt
          spdlog::spdlog
          lefticus::tools
          Jaxie::sample_library
          Jaxie::audio_capture
          Jaxie::streaming_rnnt
)

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")
