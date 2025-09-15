#include <Jaxie/onnx/streaming_rnnt.hpp>

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#if defined(JAXIE_USE_ONNXRUNTIME)
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <string>

struct streaming_rnnt_impl_core {
  Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "jaxie"};
  Ort::SessionOptions so{};
  std::unique_ptr<Ort::Session> encoder;
  std::unique_ptr<Ort::Session> predictor;
  std::unique_ptr<Ort::Session> joint;
};
#endif

namespace jaxie::onnx {

streaming_rnnt::streaming_rnnt() = default;

streaming_rnnt::~streaming_rnnt() {
#if defined(JAXIE_USE_ONNXRUNTIME)
  delete reinterpret_cast<streaming_rnnt_impl_core*>(pimpl_);
  pimpl_ = nullptr;
#endif
}

streaming_rnnt::streaming_rnnt(streaming_rnnt&& other) noexcept { *this = std::move(other); }

streaming_rnnt& streaming_rnnt::operator=(streaming_rnnt&& other) noexcept {
  if (this == &other) {
    return *this;
  }
#if defined(JAXIE_USE_ONNXRUNTIME)
  // Clean up existing owned state before taking other's
  delete reinterpret_cast<streaming_rnnt_impl_core*>(pimpl_);
  pimpl_ = other.pimpl_;
  other.pimpl_ = nullptr;
#endif
  loaded_ = other.loaded_;
  other.loaded_ = false;
  return *this;
}

bool streaming_rnnt::load(const rnnt_model_paths& paths, const ep_prefs& prefs) noexcept {
#if defined(JAXIE_USE_ONNXRUNTIME)
  // Clean up any existing state
  delete reinterpret_cast<streaming_rnnt_impl_core*>(pimpl_);
  pimpl_ = nullptr;

  auto* core = new (std::nothrow) streaming_rnnt_impl_core();
  if (core == nullptr) {
    loaded_ = false;
    return false;
  }

  // Execution Provider order preference (best-effort)
  for (const auto& prov : prefs.providers) {
    if (prov == "TensorRT" || prov == "Tensorrt" || prov == "TRT") {
#ifdef ORT_API_VERSION
      // If built with TensorRT EP available; otherwise this call is a no-op at link-time.
      // Some builds expose provider-specific appenders; keep this guarded to avoid link errors.
      // Ort::ThrowOnError is not used here; we ignore failures and continue to next EP.
      try {
        OrtTensorRTProviderOptionsV2 trt_opts{};
        // Use defaults for now.
        Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_TensorRT_V2(core->so, &trt_opts));
      } catch (...) {
        // ignore, fallback to next
      }
#endif
    } else if (prov == "CUDA" || prov == "Cuda") {
      try {
        OrtCUDAProviderOptions cuda_opts{};
        Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_CUDA(core->so, &cuda_opts));
      } catch (...) {
        // ignore
      }
    } else if (prov == "CPU" || prov == "Cpu") {
      // CPU is default; no need to append explicitly.
    }
  }

  try {
    core->encoder = std::make_unique<Ort::Session>(core->env, paths.encoder.c_str(), core->so);
    core->predictor = std::make_unique<Ort::Session>(core->env, paths.predictor.c_str(), core->so);
    core->joint = std::make_unique<Ort::Session>(core->env, paths.joint.c_str(), core->so);
  } catch (...) {
    delete core;
    loaded_ = false;
    return false;
  }

  pimpl_ = reinterpret_cast<impl*>(core);
  loaded_ = true;
  return true;
#else
  (void)paths;
  (void)prefs;
  loaded_ = false;
  return false;
#endif
}

bool streaming_rnnt::step(std::span<const float> audio_chunk, std::vector<int32_t>& emitted_tokens) const noexcept {
  (void)audio_chunk;
  (void)emitted_tokens;
  if (!loaded_) {
    return false;
  }
#if defined(JAXIE_USE_ONNXRUNTIME)
  // TODO: run encoder, predictor, joint; update states; fill out_tokens
  emitted_tokens.clear();
  return true;
#else
  return false;
#endif
}

void streaming_rnnt::reset_state() noexcept {
#if defined(JAXIE_USE_ONNXRUNTIME)
  // TODO: reset cached states
#endif
}


} // namespace jaxie::onnx
