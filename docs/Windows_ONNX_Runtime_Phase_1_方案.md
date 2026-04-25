# Windows ONNX Runtime Phase 1 方案

> 状态：Phase 1 执行文档。本文档专门管理 Windows `.pt -> .onnx -> ONNX Runtime CUDA` 算法包瘦身工作；Phase 0 的 YOLO-only 事件语义、0-based 帧号、数据库 schema、前端接口和报告字段不在本阶段重新定义。

## 目标

Phase 1 的核心目标是把 Windows 算法运行时从 `.pt + Ultralytics/PyTorch` 逐步迁移为 `.onnx + ONNX Runtime CUDA EP`，最终让算法包不再携带 `ultralytics`、`torch`、`torchvision` 等大型运行时依赖。

当前实现以实际 `.pt` 预测路径为对齐基准：`best.pt` 的 `model.args.imgsz=1024`，但 Ultralytics 在 `rect=true` 且源视频为 `960x576` 时会把输入 letterbox 成 `[1, 3, 640, 1024]`。因此 Windows Phase 1 的 ONNX 导出固定为静态 `imgsz=[640,1024]`，而不是 `640x640` 或 `1024x1024` 方形输入。

本阶段必须保持：

- 六类 YOLO-only `anomalyEvents` 语义不变。
- `startFrame/endFrame` 继续使用 0-based 闭区间。
- 事件合并规则不变。
- `videoInfo`、`performance`、`dynamicMetrics`、`globalAnalysis` 输出结构不变。
- 预处理继续使用 Python OpenCV CPU。
- 结果视频仍按 Phase 0 的 detect-only 规则绘制，不显示 track ID。

## 非目标

Phase 1 不处理以下内容：

- 不重新设计缺陷事件类别或合并规则。
- 不恢复 tracking、`objectId`、`trackingObjects`、`trajectory` 语义。
- 不调整动态指标算法。
- 不做预处理 GPU 化、C++ OpenCV、NVDEC/NVENC。
- 不改变 macOS 推理路线；macOS 后续仍以 `.pt + MPS/CPU` 为主。
- 不在 Phase A 直接移除 `.pt` 运行时。

## 版本和依赖基线

Windows Phase 1 默认面向 CUDA 12.x。

- ONNX 模型包：`onnx==1.21.0`。
- ONNX Runtime GPU 包：`onnxruntime-gpu==1.25.0`。
- Windows CPU 备用依赖使用 `onnxruntime==1.25.0`，不能与 `onnxruntime-gpu` 同时安装在同一环境。
- CUDA EP：优先 `CUDAExecutionProvider`，保留 `CPUExecutionProvider` 作为诊断 fallback。
- 当前 PyTorch CUDA 基线为 CUDA 12.x 时，ONNX Runtime GPU 需选择 CUDA 12.x / cuDNN 9.x 兼容版本。
- 运行时只允许安装 `onnxruntime` 或 `onnxruntime-gpu` 之一；Windows CUDA 包使用 `onnxruntime-gpu`。

参考依据：

- ONNX Runtime CUDA EP 官方说明：<https://onnxruntime.ai/docs/execution-providers/CUDA-ExecutionProvider.html>
- ONNX Runtime Python 安装说明：<https://onnxruntime.ai/docs/get-started/with-python.html>
- Ultralytics ONNX export 文档：<https://docs.ultralytics.com/modes/export/>

## Phase A：构建期导出和 smoke test

目标：在不改变现有 `.pt` worker 运行时的前提下，让 `npm run desktop:build-worker` 自动生成并验证 `best.onnx`。

Phase A 行为：

- 输入模型仍是 `ai-processor/weights/best.pt`。
- 构建脚本检测 `ai-processor/weights/best.onnx` 是否存在、是否落后于 `best.pt`、导出参数是否变化。
- 如需刷新，则使用构建环境中的 `ultralytics + torch` 导出 ONNX。
- 导出后用 ONNX Runtime 创建 session，并执行一帧空输入 smoke test。
- Windows CUDA 环境下必须能看到并使用 `CUDAExecutionProvider`。
- Phase A 初期仍保留 `.pt` 用于对齐；Phase C / D 完成后 Windows runtime 资源只保留 `best.onnx`。
- Phase A 不改 `desktop_worker.py` 的实际推理后端；Phase C 才切换默认 ONNX 后端。

建议冻结的导出参数：

```json
{
  "format": "onnx",
  "imgsz": [640, 1024],
  "opset": 17,
  "dynamic": false,
  "simplify": false,
  "half": false,
  "nms": false
}
```

验收：

- `npm run desktop:build-worker` 可以生成 `ai-processor/weights/best.onnx`。
- `frontend/src-tauri/resources/models/best.onnx` 被复制生成。
- ONNX Runtime smoke test 输出 provider、输入名、输入 shape、输出 shape；当前期望输入 shape 为 `[1,3,640,1024]`，输出 shape 为 `[1,10,13440]`。
- `lap` 不会被重新安装。
- 现有 `.pt` worker self-check 仍通过。

## Phase B：新增 ONNXDetector 并做跨后端对齐

目标：新增 ONNX detect-only 后端，但不立即替换桌面 worker 默认后端。

实现范围：

- 新增 `ONNXDetector`。
- 自实现 YOLO 输入预处理：
  - BGR/RGB 转换。
  - 按模型静态输入尺寸 letterbox；当前 Windows ONNX 输入为 `640x1024`，源视频 `960x576` 时缩放系数为 `1024/960`，上下 padding 为 13 像素。
  - `float32` 归一化。
  - NCHW batch。
- 自实现 YOLO 输出后处理：
  - 解析 raw ONNX 输出。
  - 置信度过滤。
  - NMS。
  - bbox 映射回原图尺寸。
  - class id / class name 映射。
- 新增 `.pt` 与 `.onnx` 检测结果对比脚本。
- `golden_samples` 同时跑 `.pt` 和 `.onnx`，比较最终 `anomalyEvents`。

验收：

- 同一视频的同类事件数量匹配。
- `startFrame/endFrame` 使用 `round(sourceVideoFps * 1.0)` 动态容差。
- `metadata.defectClass`、`metadata.maxConfidence`、`metadata.evidenceFrames` 结构不变。
- 差异必须输出可排查报告，不允许静默通过。

## Phase C：桌面 worker 切换到 ONNX Runtime CUDA

目标：Windows 桌面 worker 默认运行 `.onnx + ONNX Runtime CUDA EP`。

实现范围：

- `desktop_worker.py` 或检测模块按 Windows CUDA 包切换到 `ONNXDetector`。
- `performance.detectionBackend` 写为 `onnxruntime-cuda`。
- 打包运行时不再 import `ultralytics`、`torch`、`torchvision`。
- `best.pt` 不再进入 Windows runtime 资源包；`modelPath` 指向 `models/best.onnx`。
- 仍保留开发/验收脚本对 `.pt` 的对齐能力，但 `.pt` 不属于最终算法包运行资产。

验收：

- Windows CUDA 机器上 `CUDAExecutionProvider` 初始化成功。
- 无 GPU 或 CUDA EP 初始化失败时给出明确错误，不静默降级为 CPU 发布结果。
- golden samples 在 ONNX 后端通过。
- 结果视频和报告输出与 Phase 0 契约一致。

## Phase D：算法包资源和 DLL 收敛

目标：完成最终算法包瘦身和发布验证。

实现范围：

- 收紧 PyInstaller spec，只保留 ONNX Runtime worker 必要依赖。
- 删除 PyTorch / Ultralytics / TorchVision 运行时依赖。
- PyInstaller 后处理会把 ONNX Runtime 依赖到的 CUDA DLL 放在 worker `_internal` 根目录，并移除残留的 `torch`、`torchvision`、`ultralytics` 目录。
- 固定 ONNX Runtime CUDA EP、CUDA、cuDNN、Visual C++ runtime 依赖说明。
- 明确 CUDA / cuDNN DLL 来源、复制规则和校验方式。
- 对算法包做体积对比记录。

验收：

- `runtime/worker` 中不包含 `torch`、`torchvision`、`ultralytics`。
- `runtime/models` 中只包含 `best.onnx`。
- 打包产物能在 RTX 3090 / RTX 4090 目标环境完成分析。
- 缺失 NVIDIA 驱动、驱动过旧、CUDA DLL 缺失、显存不足时错误信息可读。

## 风险和处理原则

- `.pt` 与 `.onnx` 输出不能假设天然一致，必须以 golden samples 的事件输出为最终验收。
- ONNX 后处理要与 Ultralytics predict 的 letterbox、NMS 和 class mapping 对齐，否则事件区间会漂移。
- CUDA EP 初始化失败不能在正式 Windows CUDA 包中静默降级为 CPU。
- Phase A 可以增加临时包体，因为同时保留 `.pt` 和 `.onnx`；真正瘦身在 Phase C / D 完成。
- 每个子阶段都必须保留可回退点，避免一次性替换后难以定位差异来源。
