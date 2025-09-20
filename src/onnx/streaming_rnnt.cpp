#include <Jaxie/onnx/streaming_rnnt.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#if defined(JAXIE_USE_ONNXRUNTIME)
#include <onnxruntime_cxx_api.h>
#endif

namespace jaxie::onnx {
namespace detail {

template <typename Backend>
class rnnt_impl {
public:
  rnnt_impl() = default;
  ~rnnt_impl() { backend_.unload(); }

  rnnt_impl(const rnnt_impl&) = delete;
  rnnt_impl& operator=(const rnnt_impl&) = delete;
  rnnt_impl(rnnt_impl&&) = delete;
  rnnt_impl& operator=(rnnt_impl&&) = delete;

  bool load(const rnnt_model_paths& paths, const ep_prefs& prefs) noexcept {
    backend_.unload();
    loaded_ = backend_.load(paths, prefs);
    return loaded_;
  }

  bool step(std::span<const float> audio_chunk, std::vector<int32_t>& emitted_tokens) const noexcept {
    if (!loaded_) {
      return false;
    }
    return backend_.step(audio_chunk, emitted_tokens);
  }

  void reset() noexcept { backend_.reset(); }

  void unload() noexcept {
    backend_.unload();
    loaded_ = false;
  }

  bool is_loaded() const noexcept { return loaded_; }

private:
  mutable Backend backend_{};
  bool loaded_{false};
};

struct null_rnnt_backend {
  bool load(const rnnt_model_paths& paths, const ep_prefs& prefs) noexcept {
    static_cast<void>(paths);
    static_cast<void>(prefs);
    load_attempted_ = true;
    return false;
  }

  bool step(std::span<const float> audio_chunk, std::vector<int32_t>& emitted_tokens) const noexcept {
    static_cast<void>(audio_chunk);
    static_cast<void>(emitted_tokens);
    if (load_attempted_) {
      return false;
    }
    return false;
  }

  void reset() noexcept { load_attempted_ = false; }

  void unload() noexcept { load_attempted_ = false; }

private:
  mutable bool load_attempted_{false};
};

#if defined(JAXIE_USE_ONNXRUNTIME)

class onnx_rnnt_backend {
public:
  onnx_rnnt_backend();
  ~onnx_rnnt_backend();

  bool load(const rnnt_model_paths& paths, const ep_prefs& prefs) noexcept;
  bool step(std::span<const float> audio_chunk, std::vector<int32_t>& emitted_tokens) const noexcept;
  void reset() noexcept;
  void unload() noexcept;

private:
  void append_execution_providers(Ort::SessionOptions& options, const ep_prefs& prefs);

  Ort::Env env_;
  std::unique_ptr<Ort::Session> encoder_{};
  std::unique_ptr<Ort::Session> predictor_{};
  std::unique_ptr<Ort::Session> joint_{};
};

onnx_rnnt_backend::onnx_rnnt_backend()
  : env_(ORT_LOGGING_LEVEL_WARNING, "jaxie") {}

onnx_rnnt_backend::~onnx_rnnt_backend() { unload(); }

bool onnx_rnnt_backend::load(const rnnt_model_paths& paths, const ep_prefs& prefs) noexcept {
  unload();

  Ort::SessionOptions options{};
  append_execution_providers(options, prefs);

  try {
    encoder_ = std::make_unique<Ort::Session>(env_, paths.encoder.c_str(), options);
    predictor_ = std::make_unique<Ort::Session>(env_, paths.predictor.c_str(), options);
    joint_ = std::make_unique<Ort::Session>(env_, paths.joint.c_str(), options);
  } catch (...) {
    unload();
    return false;
  }

  return true;
}

bool onnx_rnnt_backend::step(std::span<const float> audio_chunk, std::vector<int32_t>& emitted_tokens) const noexcept {
  static_cast<void>(audio_chunk);
  if (encoder_ == nullptr || predictor_ == nullptr || joint_ == nullptr) {
    return false;
  }

  // TODO: Implement RNNT step once model IOs are finalized.
  emitted_tokens.clear();
  return true;
}

void onnx_rnnt_backend::reset() noexcept {
  // TODO: reset internal caches when RNNT state handling is implemented.
}

void onnx_rnnt_backend::unload() noexcept {
  encoder_.reset();
  predictor_.reset();
  joint_.reset();
}

void onnx_rnnt_backend::append_execution_providers(Ort::SessionOptions& options, const ep_prefs& prefs) {
  for (const auto& provider : prefs.providers) {
    if (provider == "TensorRT" || provider == "Tensorrt" || provider == "TRT") {
#if defined(ORT_API_VERSION)
      try {
        OrtTensorRTProviderOptionsV2 trt_options{};
        Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_TensorRT_V2(options, &trt_options));
      } catch (...) {
        // Ignore failures and fall back to next provider.
      }
#endif
    } else if (provider == "CUDA" || provider == "Cuda") {
      try {
        OrtCUDAProviderOptions cuda_options{};
        Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_CUDA(options, &cuda_options));
      } catch (...) {
        // Ignore failures; ONNX Runtime will fall back to CPU.
      }
    } else if (provider == "CPU" || provider == "Cpu") {
      // CPU is the default provider; nothing to append.
    }
  }
}

#endif // defined(JAXIE_USE_ONNXRUNTIME)

#if defined(JAXIE_USE_ONNXRUNTIME)
using selected_backend = onnx_rnnt_backend;
#else
using selected_backend = null_rnnt_backend;
#endif

} // namespace detail

struct streaming_rnnt::impl : detail::rnnt_impl<detail::selected_backend> {
  using base = detail::rnnt_impl<detail::selected_backend>;
  using base::base;
};

streaming_rnnt::streaming_rnnt() = default;
streaming_rnnt::~streaming_rnnt() = default;

streaming_rnnt::streaming_rnnt(streaming_rnnt&& other) noexcept
  : pimpl_(std::move(other.pimpl_)), loaded_(other.loaded_) {
  other.loaded_ = false;
}

streaming_rnnt& streaming_rnnt::operator=(streaming_rnnt&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  pimpl_ = std::move(other.pimpl_);
  loaded_ = other.loaded_;
  other.loaded_ = false;
  return *this;
}

bool streaming_rnnt::load(const rnnt_model_paths& paths, const ep_prefs& prefs) noexcept {
  if (!pimpl_) {
    try {
      pimpl_ = std::make_unique<impl>();
    } catch (...) {
      loaded_ = false;
      return false;
    }
  }

  if (!pimpl_->load(paths, prefs)) {
    loaded_ = false;
    return false;
  }

  loaded_ = true;
  return true;
}

bool streaming_rnnt::step(std::span<const float> audio_chunk, std::vector<int32_t>& emitted_tokens) const noexcept {
  if (!loaded_ || !pimpl_) {
    return false;
  }

  return pimpl_->step(audio_chunk, emitted_tokens);
}

void streaming_rnnt::reset_state() noexcept {
  if (!pimpl_) {
    return;
  }

  pimpl_->reset();
}

} // namespace jaxie::onnx
