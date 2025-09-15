
#include <string_view>
#include <string>
#include <vector>
#include <internal_use_only/config.hpp>
#include <Jaxie/onnx/streaming_rnnt.hpp>
#include <cstdlib>
#include <iostream>
#include <span>
#include <algorithm>

using std::string;
using std::string_view;

static bool has_flag(std::span<char*> args, string_view flag1, string_view flag2) {
  auto first = args.begin();
  if (first != args.end()) { ++first; }
  return std::any_of(first, args.end(), [&](const char* arg_ptr) {
    const string_view arg_sv{arg_ptr != nullptr ? arg_ptr : ""};
    return (arg_sv == flag1 || arg_sv == flag2);
  });
}

static std::vector<string> collect_ep_order(std::span<char*> args) {
  std::vector<string> order;
  for (size_t i = 1; i < args.size(); ++i) {
    const string_view arg_sv{args[i] != nullptr ? args[i] : ""};
    if (arg_sv == "--ep" && (i + 1) < args.size()) {
      const string_view prov{args[i + 1] != nullptr ? args[i + 1] : ""};
      if (!prov.empty()) {
        order.emplace_back(prov);
      }
      ++i;
    }
  }
  return order;
}

static int run_rnnt_load(std::span<char*> args, const std::vector<string>& ep_order) {
  for (size_t i = 1; i + 3 < args.size(); ++i) {
    const string_view arg_sv{args[i] != nullptr ? args[i] : ""};
    if (arg_sv == "--rnnt-load") {
      const char* enc = args[i + 1];
      const char* pred = args[i + 2];
      const char* joint = args[i + 3];
      jaxie::onnx::streaming_rnnt rnnt; // NOLINT(misc-const-correctness)
      const jaxie::onnx::rnnt_model_paths paths{
        .encoder = string(enc),
        .predictor = string(pred),
        .joint = string(joint)};
      const jaxie::onnx::ep_prefs prefs{ep_order};
#if defined(JAXIE_USE_ONNXRUNTIME)
      const bool ok = rnnt.load(paths, prefs);
      if (!ok) {
        std::cerr << "Failed to load RNNT ONNX sessions\n";
        return EXIT_FAILURE;
      }
      std::cout << "RNNT sessions loaded\n";
      return EXIT_SUCCESS;
#else
      std::cerr << "ONNX Runtime disabled at build time\n";
      return EXIT_FAILURE;
#endif
    }
  }
  return EXIT_SUCCESS;
}

int main(int argc, char** argv) noexcept
{
  try {
    const std::span<char*> args(argv, static_cast<size_t>(argc));
    if (has_flag(args, "--version", "-v")) {
      std::cout << Jaxie::cmake::project_version << '\n';
      return EXIT_SUCCESS;
    }
    if (has_flag(args, "--help", "-h")) {
      std::cout << "jaxie agent CLI\n";
      std::cout << "Usage: jaxie [--help] [--version] [--ep <CPU|CUDA|TensorRT>] --rnnt-load <encoder> <predictor> <joint>\n";
      return EXIT_SUCCESS;
    }
    const auto ep_order = collect_ep_order(args);
    const int rnnt_rc = run_rnnt_load(args, ep_order);
    if (rnnt_rc != EXIT_SUCCESS) {
      return rnnt_rc;
    }
    // No-op default.
    return EXIT_SUCCESS;
  } catch (...) {
    return EXIT_FAILURE;
  }
}
