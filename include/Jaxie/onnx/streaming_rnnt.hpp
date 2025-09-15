#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace jaxie::onnx {

struct ep_prefs {
  // Order preference: e.g., {"Tensorrt", "CUDA", "CPU"}
  std::vector<std::string> providers;
};

struct rnnt_model_paths {
  std::string encoder;
  std::string predictor;
  std::string joint;
};

class streaming_rnnt {
public:
  streaming_rnnt();
  ~streaming_rnnt();

  streaming_rnnt(const streaming_rnnt&) = delete;
  streaming_rnnt& operator=(const streaming_rnnt&) = delete;
  streaming_rnnt(streaming_rnnt&&) noexcept;
  streaming_rnnt& operator=(streaming_rnnt&&) noexcept;

  bool load(const rnnt_model_paths& paths, const ep_prefs& prefs) noexcept;

  // Run one streaming step. Interfaces will evolve once IO names and shapes are finalized.
  bool step(
    std::span<const float> audio_chunk,
    std::vector<int32_t>& emitted_tokens) const noexcept;

  void reset_state() noexcept; // clear caches/hidden states between utterances

private:
#if defined(JAXIE_USE_ONNXRUNTIME)
  struct impl;
  impl* pimpl_{}; // allocated in .cpp where ORT headers are available
#else
  // Stub for builds without ONNX Runtime
#endif
  bool loaded_{false};
};

} // namespace jaxie::onnx
