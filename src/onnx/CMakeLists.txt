add_library(streaming_rnnt STATIC streaming_rnnt.cpp)

add_library(Jaxie::streaming_rnnt ALIAS streaming_rnnt)

target_link_libraries(streaming_rnnt PRIVATE Jaxie_options Jaxie_warnings)

target_include_directories(streaming_rnnt ${WARNING_GUARD} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                                                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)

target_compile_features(streaming_rnnt PUBLIC cxx_std_23)

if(JAXIE_USE_ONNXRUNTIME)
  # User-provided cache hints
  set(ONNXRUNTIME_DIR "" CACHE PATH "Root of ONNX Runtime installation (prefix)")
  set(ONNXRUNTIME_INCLUDE_DIR "" CACHE PATH "ONNX Runtime include directory override")
  set(ONNXRUNTIME_LIB_DIR "" CACHE PATH "ONNX Runtime library directory override")
  set(ONNXRUNTIME_LIBRARY "" CACHE FILEPATH "Path to ONNX Runtime library override")

  # Try to locate ONNX Runtime via hints; if found, enable and link.
  if(NOT ONNXRUNTIME_INCLUDE_DIR)
    find_path(ONNXRUNTIME_INCLUDE_DIR
            NAMES onnxruntime_cxx_api.h
            HINTS ${ONNXRUNTIME_DIR} ENV ONNXRUNTIME_DIR ENV ONNXRUNTIME_ROOT
            PATH_SUFFIXES include include/onnxruntime)
  endif()

  if(NOT ONNXRUNTIME_LIBRARY)
    find_library(ONNXRUNTIME_LIBRARY
                 NAMES onnxruntime onnxruntime.lib
                 HINTS ${ONNXRUNTIME_LIB_DIR} ${ONNXRUNTIME_DIR} ENV ONNXRUNTIME_DIR ENV ONNXRUNTIME_ROOT
                 PATH_SUFFIXES lib lib64)
  endif()

  if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIBRARY)
    target_compile_definitions(streaming_rnnt PUBLIC JAXIE_USE_ONNXRUNTIME=1)
    target_include_directories(streaming_rnnt PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
    target_link_libraries(streaming_rnnt PRIVATE ${ONNXRUNTIME_LIBRARY})
    message(STATUS "ONNX Runtime found: ${ONNXRUNTIME_LIBRARY}")
  else()
    message(WARNING "JAXIE_USE_ONNXRUNTIME=ON but ONNX Runtime not found. Building without ORT support.")
  endif()
endif()
