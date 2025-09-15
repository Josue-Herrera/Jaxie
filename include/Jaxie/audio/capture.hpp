#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <memory>

namespace jaxie::audio {

struct capture_config {
  uint32_t sample_rate_hz{16000};
  uint32_t channels{1};
  uint32_t period_frames{160}; // ~10 ms at 16 kHz
  uint32_t period_count{3};
};

using capture_callback = std::function<void(std::span<const float>)>;

// Simple audio capture wrapper. If JAXIE_USE_MINIAUDIO is ON, impl uses miniaudio; otherwise stubs.
class audio_capture {
public:
  audio_capture();
  ~audio_capture();

  audio_capture(const audio_capture&) = delete;
  audio_capture& operator=(const audio_capture&) = delete;
  audio_capture(audio_capture&&) noexcept;
  audio_capture& operator=(audio_capture&&) noexcept;

  bool init(const capture_config& cfg, capture_callback on_frames) noexcept;
  bool start() noexcept;
  void stop() noexcept;
  void shutdown() noexcept;

  bool is_started() const noexcept { return started_; }
  capture_config current_config() const noexcept { return cfg_; }

private:
  capture_config cfg_{};
  capture_callback on_frames_{};
  bool initialized_{false};
  bool started_{false};

#if defined(JAXIE_USE_MINIAUDIO)
  struct impl;
  std::unique_ptr<impl> pimpl_{};
#else
  // Stub state for builds without miniaudio
  // We simulate a device that cannot start.
#endif
};

} // namespace jaxie::audio
