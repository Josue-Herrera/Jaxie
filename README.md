# Jaxie

[![ci](https://github.com/Josue-Herrera/Jaxie/actions/workflows/ci.yml/badge.svg)](https://github.com/Josue-Herrera/Jaxie/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/Josue-Herrera/Jaxie/branch/main/graph/badge.svg)](https://codecov.io/gh/Josue-Herrera/Jaxie)
[![CodeQL](https://github.com/Josue-Herrera/Jaxie/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/Josue-Herrera/Jaxie/actions/workflows/codeql-analysis.yml)

## About Jaxie
Create a Agent Jetson Nano Orin


## Features

- Miniaudio capture (16 kHz mono) with lock‑free ring buffer and consumer thread.
- ONNX Runtime RNNT streaming scaffold (encoder/predictor/joint) with EP order preference (TensorRT → CUDA → CPU).
- CLI utility `jaxie` with `--version`, `--help`, and RNNT load smoke test.
- Docker workflows for CPU, CUDA (x64), and Jetson (aarch64 L4T) environments.
- Tests (Catch2) covering audio capture lifecycle and CLI behavior.

## Quick Start

- CPU (Docker):
  - PowerShell: `scripts/build_docker.ps1 -Command build -Tag jaxie:latest`
  - Bash: `scripts/build_docker.sh build --tag jaxie:latest`
  - Configure + build: use `config`/`buildonly` subcommands (see Docker README).

- CUDA x64 (Docker):
  - PowerShell: `scripts/build_docker.ps1 -Command build -Tag jaxie:cuda -Cuda`
  - Bash: `scripts/build_docker.sh build --tag jaxie:cuda --cuda`

- Jetson (L4T aarch64, Docker on device):
  - Bash: `scripts/build_docker.sh build --tag jaxie:jetson --jetson`
  - Run with audio + GPU access (example): see Docker README Jetson notes.

## CLI

- Version: `jaxie --version`
- Help: `jaxie --help`
- RNNT load (with EP order):
  - `jaxie --ep TensorRT --ep CUDA --ep CPU --rnnt-load encoder.onnx predictor.onnx joint.onnx`
  - If ONNX Runtime is not found, this returns a clear error; see Building README for ORT hints.

## Tests

- Run all tests: see Docker README helper scripts or `ctest -C Release` in the build dir.
- Live audio device check: set `JAXIE_AUDIO_DEVICE_TEST=1` to require real mic capture during tests.

## Configuration

- Miniaudio and ONNX Runtime are enabled by default.
- ONNX Runtime auto‑discovery via `ONNXRUNTIME_DIR`/`ONNXRUNTIME_ROOT` or CMake cache hints. Graceful fallback if missing.

## More Details

 * [Dependency Setup](README_dependencies.md)
 * [Building Details](README_building.md)
 * [Troubleshooting](README_troubleshooting.md)
 * [Docker](README_docker.md)
