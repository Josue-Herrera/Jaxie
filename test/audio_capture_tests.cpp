// SPDX-License-Identifier: UNLICENSED
#include <Jaxie/audio/capture.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <string>
#include <span>

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

static bool get_env_flag(const char* name) {
  const char* val_c = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
  if (val_c == nullptr) {
    return false;
  }
  const std::string val(val_c);
  return (val == "1" || val == "true" || val == "TRUE");
}

TEST_CASE("audio_capture init requires callback", "[audio]") {
  jaxie::audio::audio_capture cap;
  const jaxie::audio::capture_config cfg{};
  const bool init_ok = cap.init(cfg, {});
  REQUIRE_FALSE(init_ok);
}

TEST_CASE("audio_capture lifecycle and optional device smoke", "[audio]") {
  jaxie::audio::audio_capture cap;
  const jaxie::audio::capture_config cfg{}; // 16kHz mono, ~10ms periods by default

  std::atomic<int> cb_calls{0};
  auto on_frames = [&](std::span<const float> frames) {
    // Expect period-sized frames when device is active
    (void)frames;
    cb_calls.fetch_add(1, std::memory_order_relaxed);
  };

  REQUIRE(cap.init(cfg, on_frames));

  // Idempotent stop() before start
  cap.stop();

  const bool started = cap.start();
  const bool require_device = get_env_flag("JAXIE_AUDIO_DEVICE_TEST");

  if (require_device) {
    // If explicitly asked to run the device test, require start to succeed and callbacks to arrive.
    REQUIRE(started);
    // Wait up to ~2s for at least one callback.
    constexpr int max_iters = 200;
    const auto sleep_dur = 10ms;
    for (int i = 0; i < max_iters && cb_calls.load(std::memory_order_relaxed) == 0; ++i) {
      std::this_thread::sleep_for(sleep_dur);
    }
    REQUIRE(cb_calls.load(std::memory_order_relaxed) > 0);
  }

  // Allow graceful behavior without device: just ensure no crashes and correct state transitions.
  cap.stop();
  cap.shutdown();
}
