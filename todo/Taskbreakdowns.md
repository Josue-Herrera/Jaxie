<h1>jaxie</h1>

The overall Goal of jaxie is suppose to be an character who listens and responds to your responds in an electronic pet. 

This will be developed on Windows Natively to test but ultimately it will run on Jetson Nano Orin 8gb.

Windows-native plan for miniaudio while keeping ONNX Runtime for RNNT chunked streaming. Essentially it will be 
a cross platform implementation of jaxie.

This will be a multistage project where it will ensure you have all the right dependences exists and if not it will
initialize a setup process and validation of the process.

<h2>Contributing Project Guidelines</h2>

- When adding a 3rdparty library make a static library wrapper for it and test that wrapper to ensure it will work regardless of OS.
- Use existing sample library as example for cmake or C++ examples.

<h2>Steps</h2>

<b>1. Export Parakeet-RNNT to ONNX (do this in WSL/Linux)</b>
- [ ] RNNT export typically produces multiple ONNX files (encoder / predictor / joint). That’s expected, and NeMo includes example code for ONNX RNNT inference.

<b>2. Quick validation on Windows (Python, GPU)</b>
- [ ] Use onnxruntime-gpu (CUDA EP) to validate chunking + merging logic with your exported RNNT pieces. This is the fastest way to confirm “streaming” behavior and tune chunk_len/contexts.
- [ ] CUDA EP setup & versions (CUDA 12.x)
- [ ] Create validation stragety

<b>3.Production on Windows (C++ with Visual Studio)</b>
- [ ] TensorRT-RTX EP: Windows ML/ONNX Runtime now exposes NVIDIA TensorRT RTX as a downloadable EP on RTX PCs; it can outperform CUDA EP. Start on CUDA EP, then try TRT-RTX.
	- [ ] Microsoft’s Windows ML docs show how to explicitly select EPs; copy the order pattern (e.g., TensorRT-RTX → CUDA → CPU).	
	- Note: Classic “TensorRT EP” can be built on Windows but is fiddly; try TRT-RTX EP first if you have a GeForce/RTX box; otherwise stick with CUDA EP.
- [ ] Wire up ONNX Runtime (C++)
	- [ ] Add NuGet: Microsoft.ML.OnnxRuntime.Gpu.Windows. Create one Ort::Session per RNNT component (encoder/predictor/joint).
	- [ ] Register EPs in order
- [ ] Low-latency audio IO (miniaudio)
	- [ ] Use the miniaudio samples for shared/timer-driven capture and low-latency playback. Start from these references:
	- [ ] Settings to hit low latency : 16 kHz mono PCM, 10–20 ms frames and Small buffer (shared mode) with periodic timer; double-buffer your own ring.

<b>streaming loop (C++)</b>
- [ ] Capture PCM frames into a ring buffer.
- [ ] Chunk as: left_context | chunk | right_context. Keep a rolling left cache; only right adds latency.
- [ ] Run encoder on chunk (+ contexts) and update encoder cache.
- [ ] RNNT greedy decode with predictor + joint, maintaining decoder state across chunks.
- [ ] Apply middle-token merge to stabilize and emit words; repeat. 
- The exact IO names, shapes, and state tensors come from your exported ONNX graphs—mirror the NeMo (infer_transducer_onnx.py)

<h2>How we’ll wire it</h2>

miniaudio capture → ring buffer (float32, 16 kHz mono)
- [ ] Configure a low-latency capture device with ma_device_config_init(ma_device_type_capture), config.capture.format = ma_format_f32, config.capture.channels = 1, config.sampleRate = 16000.
- [ ] Hint smaller callback periods with config.periodSizeInFrames (e.g., 160 = 10 ms) and config.periodCount = 3. They’re hints, not guarantees; backends may choose different sizes. 
- [ ] If your mic can’t run 16 kHz natively, either let miniaudio convert automatically or add a ma_resampler/ma_data_converter for explicit control. 
- [ ] Use ma_pcm_rb as the lock-free PCM ring buffer between the capture callback and your inference thread. 


Inference thread (ONNX Runtime)
- [ ] Load your RNNT encoder / predictor / joint ONNX sessions (CUDA EP first, CPU fallback).
- [ ] Pop 10–20 ms frames from the ring buffer, assemble left | chunk | right windows (e.g., chunk=0.20s, right=0.05–0.10s, left≈0.4–0.6s), run encoder + RNNT greedy decode, and apply middle-token merge to emit stable partials fast. (Latency ≈ chunk + right.)
- [ ] Use the NuGet Microsoft.ML.OnnxRuntime.Gpu.Windows package and enable the CUDA Execution Provider. 

Playback / TTS (also miniaudio)
- [ ] When you add TTS, spin up a miniaudio playback device (or keep a duplex device if you want tight round-trip tests). Examples are in the miniaudio docs. 


What to test first
File loopback: run the RNNT chunk/merge logic on WAVs to verify latency (chunk + right) and accuracy.
Live mic: confirm first partial in ~200–300 ms with chunk≈0.20s, right≈0.05–0.10s.
Stress: speak while audio is playing (when you add TTS) to check buffer health; watch for RB over/underruns.

Handy references & examples
- [ ] Manual & device config basics (init, capture/playback examples). 
- [ ] Duplex / sample-rate conversion notes & examples. 
- [ ] Ring buffer (ma_pcm_rb) usage patterns. 
- [ ] Resampling & data conversion APIs. 
- [ ] ONNX Runtime CUDA EP & Windows GPU NuGet. 
