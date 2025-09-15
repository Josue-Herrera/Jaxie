#include <Jaxie/audio/capture.hpp>

#include <atomic>
#include <cstddef>
#include <cstring>
#include <span>
#include <chrono>
#include <thread>
#include <memory>
#include <vector>
#include <utility>

#if defined(JAXIE_USE_MINIAUDIO)
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#endif

namespace jaxie::audio {

#if defined(JAXIE_USE_MINIAUDIO)
struct capture_cb_bridge {
  ma_pcm_rb* rb{nullptr};
  ma_uint32 channels{0};
};

struct audio_capture::impl {
  ma_context ctx{};
  ma_device device{};
  ma_pcm_rb rb{};
  capture_cb_bridge bridge{};
  std::thread consumer;
  std::atomic<bool> consumer_running{false};
  std::vector<float> consumer_buf; // sized to period_frames
};
#endif

audio_capture::audio_capture() = default;
audio_capture::~audio_capture() { shutdown(); }

audio_capture::audio_capture(audio_capture&& other) noexcept { *this = std::move(other); }

audio_capture& audio_capture::operator=(audio_capture&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  stop();
  shutdown();
  cfg_ = other.cfg_;
  on_frames_ = std::move(other.on_frames_);
  initialized_ = other.initialized_;
  started_ = other.started_;
#if defined(JAXIE_USE_MINIAUDIO)
  pimpl_ = std::move(other.pimpl_);
#endif
  other.initialized_ = false;
  other.started_ = false;
  return *this;
}

#if defined(JAXIE_USE_MINIAUDIO)
static void ma_capture_callback(ma_device* p_device, void* p_output, const void* p_input, ma_uint32 frame_count) { // NOLINT(*-easily-swappable-parameters)
  (void)p_output;
  if (p_device == nullptr || p_input == nullptr) {
    return;
  }

  auto* bridge = static_cast<capture_cb_bridge*>(p_device->pUserData);
  if (bridge == nullptr || bridge->rb == nullptr) {
    return;
  }

  // Write captured frames into ring buffer; if overflow, drop old data.
  ma_uint32 frames_to_write = frame_count;
  const std::span<const float> in_samples(static_cast<const float*>(p_input), static_cast<size_t>(frame_count) * p_device->capture.channels);
  size_t consumed_frames = 0;
  while (frames_to_write > 0) {
    void* p_dst = nullptr;
    ma_uint32 write_cap = 0;
    if (ma_pcm_rb_acquire_write(bridge->rb, &write_cap, &p_dst) != MA_SUCCESS) {
      break;
    }
    write_cap = (write_cap > frames_to_write) ? frames_to_write : write_cap;
    if (write_cap == 0) {
      // Ring buffer full. Drop oldest frames to make room.
      const ma_uint32 free_up = frames_to_write;
      ma_pcm_rb_seek_read(bridge->rb, free_up);
      continue;
    }
    const size_t samples = static_cast<size_t>(write_cap) * bridge->channels;
    const size_t offset = consumed_frames * static_cast<size_t>(bridge->channels);
    auto chunk = in_samples.subspan(offset, samples);
    const size_t bytes = samples * sizeof(float);
    std::memcpy(p_dst, chunk.data(), bytes);
    frames_to_write -= write_cap;
    consumed_frames += write_cap;
    ma_pcm_rb_commit_write(bridge->rb, write_cap);
  }
}
#endif

bool audio_capture::init(const capture_config& cfg, capture_callback on_frames) noexcept {
  if (!on_frames) {
    return false;
  }
  cfg_ = cfg;
  on_frames_ = std::move(on_frames);

#if defined(JAXIE_USE_MINIAUDIO)
  auto tmp = std::make_unique<impl>();

  if (ma_context_init(nullptr, 0, nullptr, &tmp->ctx) != MA_SUCCESS) {
    return false;
  }

  ma_device_config dcfg = ma_device_config_init(ma_device_type_capture);
  dcfg.capture.format = ma_format_f32;
  dcfg.capture.channels = static_cast<ma_uint32>(cfg_.channels);
  dcfg.sampleRate = static_cast<ma_uint32>(cfg_.sample_rate_hz);
  dcfg.periodSizeInFrames = static_cast<ma_uint32>(cfg_.period_frames);
  dcfg.periods = static_cast<ma_uint32>(cfg_.period_count);
  dcfg.dataCallback = ma_capture_callback;
  // Bridge gives the callback only what it needs without exposing private types.
  tmp->bridge.rb = &tmp->rb;
  tmp->bridge.channels = dcfg.capture.channels;
  dcfg.pUserData = &tmp->bridge;

  if (ma_device_init(&tmp->ctx, &dcfg, &tmp->device) != MA_SUCCESS) {
    ma_context_uninit(&tmp->ctx);
    return false;
  }

  const auto rb_frames = static_cast<ma_uint32>(cfg_.period_frames * cfg_.period_count * 8U); // generous buffer
  if (ma_pcm_rb_init(ma_format_f32, dcfg.capture.channels, rb_frames, nullptr, nullptr, &tmp->rb) != MA_SUCCESS) {
    ma_device_uninit(&tmp->device);
    ma_context_uninit(&tmp->ctx);
    return false;
  }
  try {
    tmp->consumer_buf.resize(static_cast<size_t>(cfg_.period_frames) * static_cast<size_t>(cfg_.channels));
  } catch (...) {
    ma_pcm_rb_uninit(&tmp->rb);
    ma_device_uninit(&tmp->device);
    ma_context_uninit(&tmp->ctx);
    return false;
  }

  pimpl_ = std::move(tmp);

  initialized_ = true;
  return true;
#else
  initialized_ = true; // allow start() to report meaningful error
  return true;
#endif
}

bool audio_capture::start() noexcept {
  if (!initialized_ || started_) {
    return initialized_ && started_;
  }
#if defined(JAXIE_USE_MINIAUDIO)
  if (ma_device_start(&pimpl_->device) != MA_SUCCESS) {
    return false;
  }

  pimpl_->consumer_running.store(true, std::memory_order_release);
  pimpl_->consumer = std::thread([this]() {
    auto frames_per_pull = static_cast<ma_uint32>(cfg_.period_frames);
    while (pimpl_->consumer_running.load(std::memory_order_acquire)) {
      void* p_src = nullptr;
      ma_uint32 available = 0;
      if (ma_pcm_rb_acquire_read(&pimpl_->rb, &available, &p_src) != MA_SUCCESS) {
        available = 0;
      }
      if (available >= frames_per_pull) {
        const size_t bytes = static_cast<size_t>(frames_per_pull) * static_cast<size_t>(cfg_.channels) * sizeof(float);
        std::memcpy(pimpl_->consumer_buf.data(), p_src, bytes);
        ma_pcm_rb_commit_read(&pimpl_->rb, frames_per_pull);
        // Deliver to client callback outside the realtime thread.
        on_frames_(std::span<const float>(pimpl_->consumer_buf.data(), pimpl_->consumer_buf.size()));
      } else {
        // Not enough frames; sleep briefly to avoid busy wait.
        if (available > 0) {
          ma_pcm_rb_commit_read(&pimpl_->rb, 0);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
  });

  started_ = true;
  return true;
#else
  started_ = false;
  return false;
#endif
}

void audio_capture::stop() noexcept {
  if (!started_) {
    return;
  }
#if defined(JAXIE_USE_MINIAUDIO)
  pimpl_->consumer_running.store(false, std::memory_order_release);
  if (pimpl_->consumer.joinable()) {
    pimpl_->consumer.join();
  }
  ma_device_stop(&pimpl_->device);
#endif
  started_ = false;
}

void audio_capture::shutdown() noexcept {
#if defined(JAXIE_USE_MINIAUDIO)
  if (pimpl_ != nullptr) {
    if (started_) {
      stop();
    }
    ma_pcm_rb_uninit(&pimpl_->rb);
    ma_device_uninit(&pimpl_->device);
    ma_context_uninit(&pimpl_->ctx);
    pimpl_.reset();
  }
#endif
  initialized_ = false;
  started_ = false;
}

} // namespace jaxie::audio
