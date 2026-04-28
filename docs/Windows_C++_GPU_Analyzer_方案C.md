# Windows C++ GPU Analyzer 方案 C

> 状态：已接入正式 Windows runtime 链路。当前默认任务使用 C++ GPU 预处理 sidecar、C++ ONNX Runtime CUDA 推理 sidecar、Python 事件/动态指标/结果汇总和前端 overlay；默认不再导出标注结果视频。

## 目标

- 将 Windows CUDA 侧的视频预处理和 ONNX 推理收敛到 C++ sidecar。
- 减少 Python worker 对大体积推理依赖和视频编码链路的绑定。
- 保持 YOLO-only 事件语义、0-based 帧号、类别映射和 golden sample 验收口径。
- 无 NVIDIA GPU、驱动过旧、NVDEC/NVENC 不可用、ONNX Runtime CUDA EP 初始化失败、显存不足时明确失败，不静默回退 CPU。

## 非目标

- 当前阶段不迁移事件生成、动态指标或报告生成。
- 默认分析链路不导出标注视频；标注视频导出作为后续按需功能单独实现。
- 当前阶段不实现 GPU 帧直接喂给 ONNX Runtime I/O Binding。
- 不支持 CPU fallback。

## 当前正式链路

```text
Python worker
  -> var-gpu-preprocessor.exe（仅启用预处理时）
       -> output/preprocessed.mp4
       -> preprocessingBenchmark
  -> var-video-analyzer.exe
       -> ONNX Runtime CUDA EP
       -> output/detections.json
       -> detectionBenchmark
  -> Python AnomalyEventGenerator
  -> Python MetricsCalculator
  -> Python result summary
  -> stdout NDJSON result
  -> 前端 VideoPlayer overlay
```

## Sidecar

### `var-gpu-preprocessor.exe`

职责：

- `--self-check` 输出 OpenCV、CUDA runtime、CUDA driver、GPU 名称、NVDEC/NVENC 状态。
- `--self-check` 必须同时输出 `opencvNvcodecEnabled:true`、`opencvNvcuvidBuildFlag:true`、`opencvNvencBuildFlag:true`、`opencvBuildHasNvcuvid:true`、`opencvBuildHasNvenc:true`，用于确认 OpenCV 编译产物真的启用了 `cudacodec` 的 NVDEC/NVENC 后端。
- 使用 GPU decode/preprocess/encode 生成 `preprocessed.mp4`。
- 输出 `performance.preprocessingBenchmark`。

失败策略：

- sidecar 缺失、GPU 不可用、NVDEC/NVENC 初始化失败、codec 不支持时任务失败。
- 不回退到 Python CPU 预处理。

### `var-video-analyzer.exe`

职责：

- `--self-check-onnx --model best.onnx` 验证 ONNX Runtime CUDA EP。
- 加载 `best.onnx`。
- 按冻结输入 `[1,3,640,1024]` 做 letterbox、推理、后处理、class-aware NMS 和坐标还原。
- 输出逐帧 `detections.json`。

当前 self-check 期望：

- ONNX Runtime: `1.25.0`
- provider: `CUDAExecutionProvider`
- input shape: `[1,3,640,1024]`
- output shape: `[1,10,13440]`

## Python Worker 边界

Python worker 当前保留：

- job JSON 读取。
- sidecar 调用和 progress/result NDJSON 协议。
- 异常事件生成。
- 动态指标。
- 结果汇总。

Python worker 当前不再保留：

- Python `ONNXDetector`。
- `YOLOTracker`。
- `detector_factory`。
- Python PT/ONNX fallback。
- Python CPU 预处理器。
- 默认结果视频导出。

## 打包边界

Windows CUDA runtime zip 包含：

- `desktop_worker.exe`
- `var-gpu-preprocessor.exe`
- `var-video-analyzer.exe`
- `models/best.onnx`
- OpenCV runtime DLL
- FFmpeg/ffprobe 和必要 codec DLL
- CUDA Toolkit runtime DLL
- cuDNN 9 DLL
- NPP / nvrtc 等必要 DLL
- ONNX Runtime DLL 和 CUDA provider DLL

Windows CUDA runtime zip 不包含：

- NVIDIA Driver 侧 DLL：`nvcuvid.dll`、`nvEncodeAPI64.dll`
- CPU fallback runtime
- PyTorch / Ultralytics
- Python ONNX Runtime
- SciPy / MKL
- Pillow / requests

## 当前验证摘要

| 验证项 | 结果 |
| --- | --- |
| `var-gpu-preprocessor --self-check` | 通过，识别 NVIDIA GPU、CUDA、NVDEC/NVENC，且 `opencvNvcodecEnabled:true` |
| `var-gpu-preprocessor --input golden_samples/sample_1.mp4 --output ...` | 通过，真实视频 GPU 解码/编码链路可用 |
| `var-video-analyzer --self-check-onnx --model best.onnx` | 通过，CUDA EP |
| `desktop_worker --self-check` | 通过 |
| Python `py_compile` | 通过 |
| `cargo check` | 通过 |
| `npm run lint` / `npm run typecheck` | 通过 |
| `npm run desktop:build-worker` | 通过 |
| `node scripts/build-windows-runtime-package.mjs` | 通过 |

worker 打包结果：

- `_internal` 约 20.6MB。
- 不包含 torch/onnxruntime/scipy/MKL/requests/Pillow/ultralytics。

默认任务产物：

| 产物 | 状态 |
| --- | --- |
| `output/preprocessed.mp4` | 启用预处理时生成 |
| `output/detections.json` | 生成 |
| `output/result.mp4` | 默认不生成 |
| `result_video_ready` event | 默认不发送 |

## 测试顺序

1. `npm run desktop:export-onnx`
2. `npm run desktop:build-gpu-sidecars`，该命令会执行 GPU/ONNX self-check，并用 `golden_samples/sample_1.mp4` 做 GPU 预处理真实视频烟测
3. `var-gpu-preprocessor.exe --self-check`，确认 `opencvNvcodecEnabled:true`
4. `var-video-analyzer.exe --self-check-onnx --model best.onnx`
5. `npm run desktop:build-worker`
6. `desktop_worker.exe --self-check`
7. `npm run desktop:windows:runtime`
8. 导入 runtime zip 后执行完整桌面任务验证
9. 验证 `preprocessed.mp4`、`detections.json` 和前端 overlay

## 后续工作

- 按需标注视频导出：后续单独实现按钮和后端导出命令。
- 动态指标迁移：如未来要完全移除 Python OpenCV，再评估迁到 C++ 或 Rust。
- I/O Binding：如需要进一步提升推理吞吐，再评估 GPU 帧到 ONNX Runtime 的流水线融合。
