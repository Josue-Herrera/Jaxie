## Docker Instructions

If you have [Docker](https://www.docker.com/) installed, you can run this
in your terminal, when the Dockerfile is inside the `.devcontainer` directory:

```bash
docker build -f ./.devcontainer/Dockerfile --tag=my_project:latest .
docker run -it my_project:latest
```

This command will put you in a `bash` session in a Ubuntu 20.04 Docker container,
with all of the tools listed in the [Dependencies](#dependencies) section already installed.
Additionally, you will have `g++-11` and `clang++-13` installed as the default
versions of `g++` and `clang++`.

If you want to build this container using some other versions of gcc and clang,
you may do so with the `GCC_VER` and `LLVM_VER` arguments:

```bash
docker build --tag=myproject:latest --build-arg GCC_VER=10 --build-arg LLVM_VER=11 .
```

The CC and CXX environment variables are set to GCC version 11 by default.
If you wish to use clang as your default CC and CXX environment variables, you
may do so like this:

```bash
docker build --tag=my_project:latest --build-arg USE_CLANG=1 .
```

You will be logged in as root, so you will see the `#` symbol as your prompt.
You will be in a directory that contains a copy of the `cpp_starter_project`;
any changes you make to your local copy will not be updated in the Docker image
until you rebuild it.
If you need to mount your local copy directly in the Docker image, see
[Docker volumes docs](https://docs.docker.com/storage/volumes/).
TLDR:

```bash
docker run -it \
	-v absolute_path_on_host_machine:absolute_path_in_guest_container \
	my_project:latest
```

You can configure and build [as directed above](#build) using these commands:

```bash
/starter_project# mkdir build
/starter_project# cmake -S . -B ./build
/starter_project# cmake --build ./build
```

You can configure and build using `clang-13`, without rebuilding the container,
with these commands:

```bash
/starter_project# mkdir build
/starter_project# CC=clang CXX=clang++ cmake -S . -B ./build
/starter_project# cmake --build ./build
```

The `ccmake` tool is also installed; you can substitute `ccmake` for `cmake` to
configure the project interactively.
All of the tools this project supports are installed in the Docker image;
enabling them is as simple as flipping a switch using the `ccmake` interface.
Be aware that some of the sanitizers conflict with each other, so be sure to
run them separately.

A script called `build_examples.sh` is provided to help you to build the example
GUI projects in this container.


## Helper Scripts

For a streamlined Docker workflow without touching your host toolchain, use the helper scripts included in `scripts/`.

- Windows PowerShell
  - Build image (CPU ORT): `scripts/build_docker.ps1 -Command build -Tag jaxie:latest`
  - Build image (CUDA ORT x64): `scripts/build_docker.ps1 -Command build -Tag jaxie:latest -Cuda`
  - Run shell: `scripts/build_docker.ps1 -Command run -Tag jaxie:latest`
  - Run shell with GPU: `scripts/build_docker.ps1 -Command run -Tag jaxie:latest -Cuda`
  - Configure (Release + Ninja): `scripts/build_docker.ps1 -Command config -Tag jaxie:latest`
  - Enable miniaudio at configure: `scripts/build_docker.ps1 -Command config -Tag jaxie:latest -Miniaudio`
  - Build: `scripts/build_docker.ps1 -Command buildonly -Tag jaxie:latest`
  - Test: `scripts/build_docker.ps1 -Command test -Tag jaxie:latest`

- Bash (Linux/macOS)
  - Build image (CPU ORT): `scripts/build_docker.sh build --tag jaxie:latest`
  - Build image (CUDA ORT x64): `scripts/build_docker.sh build --tag jaxie:latest --cuda`
  - Build image (Jetson L4T aarch64): `scripts/build_docker.sh build --tag jaxie:jetson --jetson`
  - Run shell: `scripts/build_docker.sh run --tag jaxie:latest`
  - Run shell with GPU: `scripts/build_docker.sh run --tag jaxie:latest --cuda`
  - Run Jetson shell with GPU: `scripts/build_docker.sh run --tag jaxie:jetson --jetson`
  - Configure (Release + Ninja): `scripts/build_docker.sh config --tag jaxie:latest`
  - Enable miniaudio at configure: `scripts/build_docker.sh config --tag jaxie:latest --miniaudio`
  - Build: `scripts/build_docker.sh buildonly --tag jaxie:latest`
  - Test: `scripts/build_docker.sh test --tag jaxie:latest`

Both scripts mount the repository at `/starter_project` in the container and run CMake/ctest inside the container, keeping your host IDE configuration unaffected.

Notes for CUDA builds:
- Requires NVIDIA Container Toolkit on the host and `--gpus all` to run.
- Default CUDA build targets x64 with ORT GPU package; Jetson (aarch64) builds typically use TensorRT or a custom ORT build. The Jetson image uses aarch64 CPU ORT by default; TensorRT/ORT GPU can be added later.

## Jetson Notes

### Audio Devices (ALSA/Pulse)
- To access microphones on the host from inside the container, pass devices and group:
  - ALSA: add `--device /dev/snd` and `--group-add audio` to `docker run`.
  - PulseAudio: export `PULSE_SERVER` and mount the socket, e.g.:
    - `-e PULSE_SERVER=unix:/run/user/1000/pulse/native -v /run/user/1000/pulse:/run/user/1000/pulse`
- Example (Bash):
  - `docker run -it --rm --gpus all --device /dev/snd --group-add audio \`
    `-e PULSE_SERVER=unix:/run/user/1000/pulse/native \`
    `-v /run/user/1000/pulse:/run/user/1000/pulse \`
    `-v "$(pwd):/starter_project" jaxie:jetson`

Miniaudio will use ALSA or Pulse backends; at least ALSA runtime (`libasound2`) is installed in the image.

### TensorRT EP for RNNT on Jetson
- ORT CPU is installed by default in the Jetson image. To use TensorRT EP (and/or CUDA EP), you typically need an ONNX Runtime build that enables TensorRT against JetPack’s libraries.
- Inside the Jetson container, use the helper to build ORT with TensorRT EP:
  - `bash scripts/build_onnxruntime_trt_jetson.sh --version v1.18.0`
  - This installs to `/opt/onnxruntime-trt` by default.
  - Configure CMake with `-DONNXRUNTIME_DIR=/opt/onnxruntime-trt`.
- CLI example to load RNNT with preference for TensorRT, then CUDA, then CPU:
  - `jaxie --ep TensorRT --ep CUDA --ep CPU --rnnt-load encoder.onnx predictor.onnx joint.onnx`

### Live Device Test Flag

The audio capture tests include an optional live device check. To require actual microphone capture and verify that callbacks are received, set the environment variable `JAXIE_AUDIO_DEVICE_TEST=1` when running tests. Example:

- Bash:
  - `docker run --rm -e JAXIE_AUDIO_DEVICE_TEST=1 -v "$(pwd):/starter_project" jaxie:latest bash -lc "ctest -C Release --test-dir /starter_project/build -L audio --output-on-failure"`
- PowerShell:
  - `docker run --rm -e JAXIE_AUDIO_DEVICE_TEST=1 -v "$PWD:/starter_project" jaxie:latest bash -lc "ctest -C Release --test-dir /starter_project/build -L audio --output-on-failure"`

If this variable is not set, the audio tests run in a device-optional mode and will pass without a live audio device.
