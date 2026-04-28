# VAR GPU Analyzer Sidecars

Windows x64 NVIDIA GPU sidecar workspace.

Current executables:

- `var-gpu-preprocessor.exe`: GPU decode/preprocess/encode sidecar. Produces `preprocessed.mp4` and `preprocessingBenchmark`.
- `var-video-analyzer.exe`: ONNX Runtime CUDA detection sidecar. Loads `best.onnx`, writes per-frame `detections.json`, and reports `detectionBenchmark`.

## Build

Run from `frontend/`:

```powershell
npm run desktop:build-gpu-sidecars
```

The build uses these defaults:

- OpenCV: `F:\Project\opencv-cpp-gpu\install-cuda-universal-nvcodec`
- CUDA Toolkit: `F:\CUDA\v12.9`
- NVIDIA Video Codec SDK: `C:\Video_Codec_SDK_13.0.37`
- ONNX Runtime GPU package: `deps/onnxruntime-gpu-windows-1.25.0`

Override with environment variables if needed:

```powershell
$env:VAR_OPENCV_ROOT='F:\Project\opencv-cpp-gpu\install-cuda-universal-nvcodec'
$env:VAR_CUDA_ROOT='F:\CUDA\v12.9'
$env:VAR_VIDEO_CODEC_SDK_ROOT='C:\Video_Codec_SDK_13.0.37'
$env:VAR_ONNXRUNTIME_ROOT='F:\Project\var-system\frontend\deps\onnxruntime-gpu-windows-1.25.0'
npm run desktop:build-gpu-sidecars
```

`desktop:build-gpu-sidecars` also copies required runtime DLLs into `frontend/src-tauri/resources/runtime/windows-x64/tools` and runs:

```powershell
var-gpu-preprocessor.exe --self-check
var-gpu-preprocessor.exe --input golden_samples\sample_1.mp4 --output preprocess-smoke.mp4 --fps 25.0
var-video-analyzer.exe --self-check-onnx --model ..\..\..\models\best.onnx
```

The GPU preprocessor self-check must report `ok:true` and `opencvNvcodecEnabled:true`. The smoke run verifies that OpenCV `cudacodec` can create the real NVDEC/NVENC reader/writer path, not just detect a CUDA device.

## CLI

```powershell
var-gpu-preprocessor.exe --self-check
var-gpu-preprocessor.exe --input input.mp4 --output preprocessed.mp4 --fps 25

var-video-analyzer.exe --self-check-onnx --model best.onnx
var-video-analyzer.exe --input input.mp4 --model best.onnx --output-detections detections.json --conf 0.5 --iou 0.45
```

Both sidecars emit NDJSON to stdout.
