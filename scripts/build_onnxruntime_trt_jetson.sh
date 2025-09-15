#!/usr/bin/env bash
set -euo pipefail

# Build ONNX Runtime with TensorRT EP on Jetson (aarch64)
# This script is intended to run inside the Jetson Docker image.

usage() {
  cat <<'USAGE'
Usage: scripts/build_onnxruntime_trt_jetson.sh [options]

Options:
  --version <v>     ORT version tag (e.g., v1.18.0). Default: v1.18.0
  --jobs <N>        Parallel jobs. Default: nproc
  --src <dir>       Source dir (default: /opt/onnxruntime-src)
  --out <dir>       Install dir (default: /opt/onnxruntime-trt)

Notes:
  - Requires CUDA, cuDNN, and TensorRT from JetPack (paths typically:
      CUDA: /usr/local/cuda
      cuDNN: /usr/lib/aarch64-linux-gnu
      TensorRT: /usr/src/tensorrt)
  - This builds the shared C++ library with TensorRT and CUDA EPs.
  - After success, set ONNXRUNTIME_DIR to <out> to enable in CMake.
USAGE
}

ORT_TAG="v1.18.0"
JOBS="$(nproc)"
SRC_DIR="/opt/onnxruntime-src"
OUT_DIR="/opt/onnxruntime-trt"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --version) ORT_TAG="$2"; shift 2 ;;
    --jobs) JOBS="$2"; shift 2 ;;
    --src) SRC_DIR="$2"; shift 2 ;;
    --out) OUT_DIR="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
  esac
done

set -x
mkdir -p "${SRC_DIR}" "${OUT_DIR}"

if [[ ! -d "${SRC_DIR}/.git" ]]; then
  git clone --recursive https://github.com/microsoft/onnxruntime.git "${SRC_DIR}"
fi
pushd "${SRC_DIR}"
git fetch --tags
git checkout "${ORT_TAG}"
git submodule sync --recursive
git submodule update --init --recursive

# Build with TensorRT + CUDA
./build.sh \
  --config Release \
  --update \
  --build \
  --build_shared_lib \
  --parallel \
  --cmake_extra_defines CMAKE_INSTALL_PREFIX=${OUT_DIR} \
  --use_tensorrt \
  --tensorrt_home=/usr/src/tensorrt \
  --use_cuda \
  --cuda_home=/usr/local/cuda \
  --cudnn_home=/usr/lib/aarch64-linux-gnu \
  --allow_running_as_root

cmake --install build/Linux/Release
popd

echo "ONNX Runtime with TensorRT installed to ${OUT_DIR}"
echo "Set -DONNXRUNTIME_DIR=${OUT_DIR} for CMake configure."

