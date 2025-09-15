add_library(audio_capture STATIC capture.cpp)

add_library(Jaxie::audio_capture ALIAS audio_capture)

target_link_libraries(audio_capture PRIVATE Jaxie_options Jaxie_warnings)

target_include_directories(audio_capture ${WARNING_GUARD} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                                                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)

target_compile_features(audio_capture PUBLIC cxx_std_23)

if(JAXIE_USE_MINIAUDIO)
  target_compile_definitions(audio_capture PUBLIC JAXIE_USE_MINIAUDIO=1)
  # Provide access to miniaudio's header and any needed flags
  if(TARGET miniaudio::miniaudio)
    target_link_libraries(audio_capture PRIVATE miniaudio::miniaudio)
  endif()
endif()
