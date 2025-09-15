#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/build_docker.sh <command> [options]

Commands:
  build       Build Docker image (uses ./.devcontainer/Dockerfile)
  run         Run shell in container with repo mounted at /starter_project
  config      Run CMake configure inside container (Release + Ninja)
  buildonly   Build existing CMake build dir inside container
  test        Run ctest inside container

Common options:
  --tag <name>           Docker image tag (default: jaxie:latest)
  --cuda                 Use CUDA base image + ORT GPU (x64)
  --arch <x64|aarch64>   Target arch for ORT package (default: x64)
  --jetson               Use Jetson L4T-specific Dockerfile (aarch64 ORT CPU)

build options:
  --gcc <ver>            GCC version build-arg (e.g., 11)
  --llvm <ver>           LLVM/Clang version build-arg (e.g., 13)
  --clang                Use Clang as default compiler in image

config options:
  --miniaudio            Enable JAXIE_USE_MINIAUDIO=ON
  --extra "..."          Extra args appended to CMake configure

Examples:
  scripts/build_docker.sh build --tag jaxie:latest --gcc 11 --llvm 13
  scripts/build_docker.sh run --tag jaxie:latest
  scripts/build_docker.sh config --tag jaxie:latest --miniaudio
  scripts/build_docker.sh buildonly --tag jaxie:latest
  scripts/build_docker.sh test --tag jaxie:latest
USAGE
}

cmd=${1:-}
shift || true

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TAG="jaxie:latest"
EXTRA=""
USE_CLANG=0
GCC_VER=""
LLVM_VER=""
ENABLE_MINIAUDIO=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --tag) TAG="$2"; shift 2 ;;
    --cuda) CUDA=1; shift ;;
    --arch) ARCH="$2"; shift 2 ;;
    --jetson) JETSON=1; shift ;;
    --gcc) GCC_VER="$2"; shift 2 ;;
    --llvm) LLVM_VER="$2"; shift 2 ;;
    --clang) USE_CLANG=1; shift ;;
    --extra) EXTRA="$2"; shift 2 ;;
    --miniaudio) ENABLE_MINIAUDIO=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
  esac
done

case "${cmd}" in
  build)
    if [[ ${JETSON:-0} -eq 1 ]]; then
      dfile="${ROOT_DIR}/.devcontainer/Dockerfile.jetson"
    else
      dfile="${ROOT_DIR}/.devcontainer/Dockerfile"
    fi
    args=( -f "${dfile}" -t "${TAG}" "${ROOT_DIR}" )
    [[ -n "${GCC_VER}" ]] && args=( --build-arg "GCC_VER=${GCC_VER}" "${args[@]}" )
    [[ -n "${LLVM_VER}" ]] && args=( --build-arg "LLVM_VER=${LLVM_VER}" "${args[@]}" )
    [[ ${USE_CLANG} -eq 1 ]] && args=( --build-arg USE_CLANG=1 "${args[@]}" )
    # CUDA/GPU build: switch base image and ORT flavor
    if [[ ${CUDA:-0} -eq 1 && ${JETSON:-0} -eq 0 ]]; then
      # Match Ubuntu 20.04 variant
      args=( --build-arg "BASE_IMAGE=nvidia/cuda:12.2.2-cudnn8-devel-ubuntu20.04" --build-arg "ORT_FLAVOR=gpu" --build-arg "ARCH=${ARCH:-x64}" "${args[@]}" )
    else
      if [[ ${JETSON:-0} -eq 1 ]]; then
        args=( --build-arg "ARCH=aarch64" --build-arg "ORT_FLAVOR=cpu" "${args[@]}" )
      else
        args=( --build-arg "ORT_FLAVOR=cpu" --build-arg "ARCH=${ARCH:-x64}" "${args[@]}" )
      fi
    fi
    echo "> docker build ${args[*]}"
    docker build "${args[@]}"
    ;;

  run)
    echo "> docker run -it --rm -v ${ROOT_DIR}:/starter_project ${TAG}"
    if [[ ${CUDA:-0} -eq 1 || ${JETSON:-0} -eq 1 ]]; then
      docker run -it --rm --gpus all -v "${ROOT_DIR}:/starter_project" "${TAG}"
    else
      docker run -it --rm -v "${ROOT_DIR}:/starter_project" "${TAG}"
    fi
    ;;

  config)
    cmake_flags=( -S /starter_project -B /starter_project/build -G Ninja -DCMAKE_BUILD_TYPE=Release )
    [[ ${ENABLE_MINIAUDIO} -eq 1 ]] && cmake_flags+=( -DJAXIE_USE_MINIAUDIO=ON )
    if [[ -n "${EXTRA}" ]]; then cmake_flags+=( ${EXTRA} ); fi
    echo "> docker run --rm ${TAG} cmake ${cmake_flags[*]}"
    docker run --rm -v "${ROOT_DIR}:/starter_project" "${TAG}" bash -lc "cmake ${cmake_flags[*]}"
    ;;

  buildonly)
    echo "> building /starter_project/build"
    docker run --rm -v "${ROOT_DIR}:/starter_project" "${TAG}" bash -lc "cmake --build /starter_project/build --parallel"
    ;;

  test)
    echo "> ctest Release"
    docker run --rm -v "${ROOT_DIR}:/starter_project" "${TAG}" bash -lc "ctest -C Release --output-on-failure --test-dir /starter_project/build --parallel"
    ;;

  *)
    usage; exit 1 ;;
esac
