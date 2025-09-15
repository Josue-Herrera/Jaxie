param(
  [ValidateSet('build','run','config','buildonly','test')]
  [string]$Command = 'build',
  [string]$Tag = 'jaxie:latest',
  [string]$Gcc = '',
  [string]$Llvm = '',
  [switch]$Clang,
  [switch]$Cuda,
  [ValidateSet('x64','aarch64')]
  [string]$Arch = 'x64',
  [switch]$Jetson,
  [switch]$Miniaudio,
  [string]$Extra = ''
)

$ErrorActionPreference = 'Stop'

function Show-Usage {
  @'
Usage: scripts\build_docker.ps1 -Command <build|run|config|buildonly|test> [options]

Options:
  -Tag <name>         Docker image tag (default: jaxie:latest)
  -Gcc <ver>          GCC version build-arg, e.g. 11
  -Llvm <ver>         LLVM/Clang version build-arg, e.g. 13
  -Clang              Use Clang as default compiler in image
  -Miniaudio          Enable JAXIE_USE_MINIAUDIO=ON for config
  -Extra "..."        Extra args appended to CMake configure

Examples:
  scripts\build_docker.ps1 -Command build -Tag jaxie:latest -Gcc 11 -Llvm 13
  scripts\build_docker.ps1 -Command run -Tag jaxie:latest
  scripts\build_docker.ps1 -Command config -Tag jaxie:latest -Miniaudio
  scripts\build_docker.ps1 -Command buildonly -Tag jaxie:latest
  scripts\build_docker.ps1 -Command test -Tag jaxie:latest
'@
}

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

switch ($Command) {
  'build' {
    if ($Jetson) { $dfile = (Join-Path $RepoRoot '.devcontainer/Dockerfile.jetson') } else { $dfile = (Join-Path $RepoRoot '.devcontainer/Dockerfile') }
    $args = @('-f', $dfile)
    if ($Gcc) { $args += @('--build-arg', "GCC_VER=$Gcc") }
    if ($Llvm) { $args += @('--build-arg', "LLVM_VER=$Llvm") }
    if ($Clang) { $args += @('--build-arg', 'USE_CLANG=1') }
    if ($Cuda -and (-not $Jetson)) {
      $args += @('--build-arg','BASE_IMAGE=nvidia/cuda:12.2.2-cudnn8-devel-ubuntu20.04','--build-arg','ORT_FLAVOR=gpu','--build-arg',"ARCH=$Arch")
    } else {
      if ($Jetson) {
        $args += @('--build-arg','ARCH=aarch64','--build-arg','ORT_FLAVOR=cpu')
      } else {
        $args += @('--build-arg','ORT_FLAVOR=cpu','--build-arg',"ARCH=$Arch")
      }
    }
    $args += @('-t', $Tag, $RepoRoot)
    Write-Host "> docker build $($args -join ' ')"
    docker build @args
  }

  'run' {
    Write-Host "> docker run -it --rm -v `"$RepoRoot`":/starter_project $Tag"
    if ($Cuda -or $Jetson) {
      docker run -it --rm --gpus all -v "$RepoRoot:/starter_project" $Tag
    } else {
      docker run -it --rm -v "$RepoRoot:/starter_project" $Tag
    }
  }

  'config' {
    $cmakeFlags = @('-S','/starter_project','-B','/starter_project/build','-G','Ninja','-DCMAKE_BUILD_TYPE=Release')
    if ($Miniaudio) { $cmakeFlags += '-DJAXIE_USE_MINIAUDIO=ON' }
    if ($Extra) { $cmakeFlags += $Extra }
    $cmd = "cmake $($cmakeFlags -join ' ')"
    Write-Host "> docker run --rm -v `"$RepoRoot`":/starter_project $Tag bash -lc `"$cmd`""
    docker run --rm -v "$RepoRoot:/starter_project" $Tag bash -lc "$cmd"
  }

  'buildonly' {
    $cmd = 'cmake --build /starter_project/build --parallel'
    docker run --rm -v "$RepoRoot:/starter_project" $Tag bash -lc "$cmd"
  }

  'test' {
    $cmd = 'ctest -C Release --output-on-failure --test-dir /starter_project/build --parallel'
    docker run --rm -v "$RepoRoot:/starter_project" $Tag bash -lc "$cmd"
  }

  default {
    Show-Usage
    exit 1
  }
}
