#include <Jaxie/audio/capture.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#if defined(JAXIE_USE_MINIAUDIO)
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#endif

namespace jaxie::audio {
namespace detail {

template <typename Backend>
class capture_impl {
public:
  capture_impl() = default;

  bool init(audio_capture& owner, const capture_config& config, capture_callback& callback) noexcept {
    return backend_.init(owner, config, callback);
  }

  bool start(audio_capture& owner, capture_callback& callback) noexcept {
    return backend_.start(owner, callback);
  }

  void stop(audio_capture& owner) noexcept { backend_.stop(owner); }

  void shutdown(audio_capture& owner) noexcept { backend_.shutdown(owner); }

private:
  Backend backend_{};
};

struct null_capture_backend {
  bool init(audio_capture& owner, const capture_config& config, capture_callback& callback) noexcept {
    last_owner_ = &owner;
    last_config_ = config;
    callback_ptr_ = &callback;
    return true;
  }

  bool start(audio_capture& owner, capture_callback& callback) noexcept {
    last_owner_ = &owner;
    callback_ptr_ = &callback;
    return false;
  }

  void stop(audio_capture& owner) noexcept { last_owner_ = &owner; }

  void shutdown(audio_capture& owner) noexcept {
    last_owner_ = &owner;
    callback_ptr_ = nullptr;
  }

private:
  audio_capture* last_owner_{nullptr};
  capture_config last_config_{};
  capture_callback* callback_ptr_{nullptr};
};

#if defined(JAXIE_USE_MINIAUDIO)

class miniaudio_capture_backend {
public:
  miniaudio_capture_backend() = default;
  ~miniaudio_capture_backend() { shutdown_internal(); }

  miniaudio_capture_backend(const miniaudio_capture_backend&) = delete;
  miniaudio_capture_backend& operator=(const miniaudio_capture_backend&) = delete;
  miniaudio_capture_backend(miniaudio_capture_backend&&) = delete;
  miniaudio_capture_backend& operator=(miniaudio_capture_backend&&) = delete;

  bool init(audio_capture& owner, const capture_config& config, capture_callback& callback) noexcept {
    shutdown_internal();
    owner_ = &owner;
    callback_ = &callback;
    config_ = config;
    channels_ = static_cast<ma_uint32>(config.channels);

    if (ma_context_init(nullptr, 0, nullptr, &ctx_) != MA_SUCCESS) {
      return false;
    }
    context_ready_ = true;

    ma_device_config dcfg = ma_device_config_init(ma_device_type_capture);
    dcfg.capture.format = ma_format_f32;
    dcfg.capture.channels = channels_;
    dcfg.sampleRate = static_cast<ma_uint32>(config.sample_rate_hz);
    dcfg.periodSizeInFrames = static_cast<ma_uint32>(config.period_frames);
    dcfg.periods = static_cast<ma_uint32>(config.period_count);
    dcfg.dataCallback = &miniaudio_capture_backend::ma_capture_callback;
    dcfg.pUserData = this;

    if (ma_device_init(&ctx_, &dcfg, &device_) != MA_SUCCESS) {
      shutdown_internal();
      return false;
    }
    device_ready_ = true;

    const auto rb_frames = static_cast<ma_uint32>(config.period_frames * config.period_count * 8U);
    if (ma_pcm_rb_init(ma_format_f32, channels_, rb_frames, nullptr, nullptr, &rb_) != MA_SUCCESS) {
      shutdown_internal();
      return false;
    }
    rb_ready_ = true;

    try {
      consumer_buf_.resize(static_cast<size_t>(config.period_frames) * static_cast<size_t>(config.channels));
    } catch (...) {
      shutdown_internal();
      return false;
    }

    return true;
  }

  bool start(audio_capture& owner, capture_callback& callback) noexcept {
    owner_ = &owner;
    callback_ = &callback;
    if (!device_ready_) {
      return false;
    }

    if (ma_device_start(&device_) != MA_SUCCESS) {
      return false;
    }
    device_running_ = true;

    consumer_running_.store(true, std::memory_order_release);
    try {
      consumer_ = std::thread([this]() { consume_loop(); });
    } catch (...) {
      consumer_running_.store(false, std::memory_order_release);
      ma_device_stop(&device_);
      device_running_ = false;
      return false;
    }

    return true;
  }

  void stop(audio_capture& owner) noexcept {
    owner_ = &owner;
    stop_internal();
  }

  void shutdown(audio_capture& owner) noexcept {
    owner_ = &owner;
    shutdown_internal();
  }

private:
  static void ma_capture_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) { // NOLINT(*-easily-swappable-parameters)
    static_cast<void>(output);
    if (device == nullptr || input == nullptr) {
      return;
    }
    auto* self = static_cast<miniaudio_capture_backend*>(device->pUserData);
    if (self == nullptr) {
      return;
    }
    self->push_samples(static_cast<const float*>(input), frame_count);
  }

  void consume_loop() {
    const auto frames_per_pull = static_cast<ma_uint32>(config_.period_frames);
    while (consumer_running_.load(std::memory_order_acquire)) {
      void* src = nullptr;
      ma_uint32 available = 0;
      if (ma_pcm_rb_acquire_read(&rb_, &available, &src) != MA_SUCCESS) {
        available = 0;
      }
      if (available >= frames_per_pull) {
        const size_t sample_count = static_cast<size_t>(frames_per_pull) * static_cast<size_t>(channels_);
        const size_t bytes = sample_count * sizeof(float);
        std::memcpy(consumer_buf_.data(), src, bytes);
        ma_pcm_rb_commit_read(&rb_, frames_per_pull);
        if (callback_ != nullptr && *callback_) {
          (*callback_)(std::span<const float>(consumer_buf_.data(), consumer_buf_.size()));
        }
      } else {
        if (available > 0) {
          ma_pcm_rb_commit_read(&rb_, 0);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
  }

  void push_samples(const float* samples, ma_uint32 frame_count) {
    if (samples == nullptr || !rb_ready_) {
      return;
    }

    const std::span<const float> in_span(samples, static_cast<size_t>(frame_count) * static_cast<size_t>(channels_));
    ma_uint32 frames_to_write = frame_count;
    size_t consumed_frames = 0;
    while (frames_to_write > 0) {
      void* dst = nullptr;
      ma_uint32 writable = 0;
      if (ma_pcm_rb_acquire_write(&rb_, &writable, &dst) != MA_SUCCESS) {
        break;
      }
      writable = (std::min)(writable, frames_to_write);
      if (writable == 0) {
        ma_pcm_rb_seek_read(&rb_, frames_to_write);
        continue;
      }
      const size_t sample_count = static_cast<size_t>(writable) * static_cast<size_t>(channels_);
      const size_t offset = consumed_frames * static_cast<size_t>(channels_);
      std::memcpy(dst, in_span.subspan(offset, sample_count).data(), sample_count * sizeof(float));
      frames_to_write -= writable;
      consumed_frames += writable;
      ma_pcm_rb_commit_write(&rb_, writable);
    }
  }

  void stop_internal() noexcept {
    consumer_running_.store(false, std::memory_order_release);
    if (consumer_.joinable()) {
      consumer_.join();
    }
    if (device_ready_ && device_running_) {
      ma_device_stop(&device_);
      device_running_ = false;
    }
  }

  void shutdown_internal() noexcept {
    stop_internal();
    if (rb_ready_) {
      ma_pcm_rb_uninit(&rb_);
      rb_ready_ = false;
    }
    if (device_ready_) {
      ma_device_uninit(&device_);
      device_ready_ = false;
    }
    if (context_ready_) {
      ma_context_uninit(&ctx_);
      context_ready_ = false;
    }
    callback_ = nullptr;
    owner_ = nullptr;
    consumer_buf_.clear();
    config_ = {};
    channels_ = 0;
  }

  ma_context ctx_{};
  ma_device device_{};
  ma_pcm_rb rb_{};
  std::thread consumer_;
  std::atomic<bool> consumer_running_{false};
  std::vector<float> consumer_buf_;
  capture_callback* callback_{nullptr};
  capture_config config_{};
  audio_capture* owner_{nullptr};
  ma_uint32 channels_{0};
  bool context_ready_{false};
  bool device_ready_{false};
  bool device_running_{false};
  bool rb_ready_{false};
};

#endif // defined(JAXIE_USE_MINIAUDIO)

#if defined(JAXIE_USE_MINIAUDIO)
using selected_backend = miniaudio_capture_backend;
#else
using selected_backend = null_capture_backend;
#endif

} // namespace detail

struct audio_capture::impl : detail::capture_impl<detail::selected_backend> {
  using base = detail::capture_impl<detail::selected_backend>;
  using base::base;
};

audio_capture::audio_capture() = default;
audio_capture::~audio_capture() { shutdown(); }

audio_capture::audio_capture(audio_capture&& other) noexcept
  : cfg_(other.cfg_),
    on_frames_(std::move(other.on_frames_)),
    initialized_(other.initialized_),
    started_(other.started_),
    pimpl_(std::move(other.pimpl_)) {
  other.initialized_ = false;
  other.started_ = false;
}

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
  pimpl_ = std::move(other.pimpl_);

  other.initialized_ = false;
  other.started_ = false;

  return *this;
}

bool audio_capture::init(const capture_config& cfg, capture_callback on_frames) noexcept {
  if (!on_frames) {
    return false;
  }

  stop();
  if (initialized_) {
    shutdown();
  }

  cfg_ = cfg;
  on_frames_ = std::move(on_frames);

  if (!pimpl_) {
    try {
      pimpl_ = std::make_unique<impl>();
    } catch (...) {
      on_frames_ = {};
      initialized_ = false;
      started_ = false;
      return false;
    }
  }

  if (!pimpl_->init(*this, cfg_, on_frames_)) {
    on_frames_ = {};
    initialized_ = false;
    started_ = false;
    return false;
  }

  initialized_ = true;
  started_ = false;
  return true;
}

bool audio_capture::start() noexcept {
  if (!initialized_ || started_) {
    return initialized_ && started_;
  }
  if (!pimpl_) {
    return false;
  }

  if (!pimpl_->start(*this, on_frames_)) {
    return false;
  }

  started_ = true;
  return true;
}

void audio_capture::stop() noexcept {
  if (!started_) {
    return;
  }
  if (pimpl_) {
    pimpl_->stop(*this);
  }
  started_ = false;
}

void audio_capture::shutdown() noexcept {
  if (!pimpl_) {
    initialized_ = false;
    started_ = false;
    on_frames_ = {};
    return;
  }

  if (started_) {
    stop();
  }

  pimpl_->shutdown(*this);
  initialized_ = false;
  started_ = false;
  on_frames_ = {};
}

} // namespace jaxie::audio
