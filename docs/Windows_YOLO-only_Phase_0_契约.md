# Windows YOLO-only Phase 0 契约

> 状态：Phase 0 冻结稿。本文档用于约束后续 Windows 优先改造，不代表当前代码已完成实现。

## 目标

Phase 0 只冻结 YOLO-only 事件语义和验收口径，不引入 ONNX Runtime，不处理预处理 GPU 加速。

当前优先顺序调整为：

1. 先把现有“追踪驱动”的分析链路改成“YOLO-only 检测驱动”。
2. 先沿用现有 Python / Ultralytics / `.pt` 推理生态完成闭环。
3. ONNX Runtime、Windows CUDA EP 发布优化、预处理 GPU 加速都后置。

本阶段明确：

- 缺陷事件直接由 YOLO 检测结果生成，不再依赖追踪对象。
- Windows 作为当前主要验证平台。
- Windows Phase 0 统一采用 0-based frame index。
- 动态指标暂不改动，继续沿用现有指标输出。
- ONNX 不作为 Phase 0 输入、输出或验收前提。

## 非目标

Phase 0 不处理以下内容：

- 不实现 ONNX Runtime 推理代码。
- 不要求 `best.onnx` 进入 runtime 包。
- 不冻结 ONNX 导出参数。
- 不做 ONNX 与 `.pt` 的跨后端一致性验收。
- 不重写预处理为 GPU / CUDA / C++。
- 不调整动态指标算法。
- 不做 macOS 发布策略调整。

## 当前推理范围

Phase 0 允许继续沿用当前 `.pt + Ultralytics/PyTorch` 路线，只要求从 tracking 改为 detect-only。

| 平台 | 当前优先级 | 模型资产 | 推理方式 | 本阶段目标 |
| --- | --- | --- | --- | --- |
| Windows x64 | 最高 | `best.pt` | Ultralytics detect-only | 先完成 YOLO-only 事件闭环 |
| macOS | 后续同步 | `best.pt` | Ultralytics detect-only | 保持 `.pt + MPS/CPU` 方向 |

说明：

- Windows Phase 0 不考虑 ONNX Runtime。
- 现有 Windows PyTorch CUDA 能力可以继续使用，但不是本阶段文档重点。
- 后续若进入 ONNX 优化阶段，再单独冻结 `best.onnx`、CUDA EP、runtime 包布局和跨后端 golden sample 比对。

## 缺陷类别契约

Phase 0 直接冻结为六类 YOLO-only 事件。

| YOLO class id | 模型类别名 | `eventType` | 展示名称 |
| --- | --- | --- | --- |
| 0 | 熔池未到边 | `POOL_NOT_REACHED` | 熔池未到边 |
| 1 | 电极粘连物 | `ADHESION` | 电极粘连物 |
| 2 | 锭冠 | `CROWN` | 锭冠 |
| 3 | 辉光 | `GLOW` | 辉光 |
| 4 | 边弧（侧弧） | `SIDE_ARC` | 边弧（侧弧） |
| 5 | 爬弧 | `CREEPING_ARC` | 爬弧 |

废弃旧事件语义：

- `ADHESION_FORMED`
- `ADHESION_DROPPED`
- `CROWN_DROPPED`

“掉入熔池”判断不进入新事件契约。

## 帧号契约

Windows Phase 0 统一采用 0-based frame index。

- 第一帧为 `0`。
- 最后一帧为 `totalFrames - 1`。
- 证据帧 `f` 的合法范围为 `[0, totalFrames - 1]`。
- `startFrame` 和 `endFrame` 都使用闭区间。
- `startTime = startFrame / sourceVideoFps`。
- `endTime = endFrame / sourceVideoFps`。
- 时间展示可按前端需要四舍五入，但验收优先比较帧号。

## YOLO-only 事件生成规则

输入：

- 逐帧 YOLO 检测结果。
- `sourceVideoFps`。
- `totalFrames`。
- 六类缺陷集合。

处理规则：

1. 遍历每帧检测结果。
2. 仅保留六类缺陷。
3. 若某帧存在某类别检测框，且置信度达到阈值，则该帧成为该类别的证据帧。
4. 对每个证据帧 `f` 生成该类别区间：

```text
radius = round(sourceVideoFps * 1.0)
startFrame = max(0, f - radius)
endFrame = min(totalFrames - 1, f + radius)
```

5. 仅在同类别内部按 `startFrame` 排序并合并。
6. 同类别区间满足以下条件时合并：

```text
next.startFrame <= current.endFrame + 1
```

7. 不同类别之间永不合并，即使时间范围重叠也保留独立事件。
8. 合并后的事件需要保留解释信息：
   - `defectClass`
   - `maxConfidence`
   - `evidenceFrames`

## 结果输出契约

Phase 0 建议结果结构如下，后续实现以此为目标。

```json
{
  "status": "COMPLETED",
  "isTimeout": false,
  "videoInfo": {
    "sourceVideoFps": 25.0,
    "totalFrames": 15000,
    "width": 1920,
    "height": 1080
  },
  "performance": {
    "preprocessingAverageFps": null,
    "defectDetectionAverageFps": 72.3,
    "preprocessingDurationSeconds": 0,
    "defectDetectionDurationSeconds": 207,
    "detectionBackend": "pytorch-cuda"
  },
  "dynamicMetrics": [],
  "globalAnalysis": {},
  "anomalyEvents": [
    {
      "eventType": "ADHESION",
      "startFrame": 100,
      "endFrame": 160,
      "startTime": 4.0,
      "endTime": 6.4,
      "metadata": {
        "defectClass": "ADHESION",
        "maxConfidence": 0.91,
        "evidenceFrames": [125, 126, 127]
      }
    }
  ]
}
```

说明：

- `detectionBackend` 在 Phase 0 不绑定 ONNX，可先记录当前实际后端，例如 `pytorch-cuda`、`pytorch-cpu`、`pytorch-mps`。
- 预处理关闭时，`preprocessingAverageFps` 为 `null`。
- 动态指标暂不改动，继续沿用当前 `dynamicMetrics` 与 `globalAnalysis` 的含义和结构。
- `anomalyEvents` 不包含 `objectId`。
- 结果不包含 `trackingObjects`。

## Golden Sample 验收契约

当前样本位于：

```text
golden_samples/
```

已存在样本：

| 样本 | 视频 | 标注 |
| --- | --- | --- |
| sample_1 | `sample_1.mp4` | `sample_1.json` |
| sample_2 | `sample_2.mp4` | `sample_2.json` |
| sample_3 | `sample_3.mp4` | `sample_3.json` |
| sample_4 | `sample_4.mp4` | `sample_4.json` |

验收比较优先级：

1. `eventType` 必须匹配。
2. 同类别事件数量必须匹配。
3. `startFrame`、`endFrame` 按 0-based 帧号比较。
4. 帧号容差建议先固定为 `±25` 帧，即 25 FPS 下约等于 `±1` 秒。
5. `startTime`、`endTime` 仅作为辅助展示字段，不作为第一优先级断言。

如果后续发现 `sourceVideoFps` 非 25 FPS，容差可按 `round(sourceVideoFps * 1.0)` 计算，但必须在验收脚本中显式记录。

## 后续优化顺序

Phase 0 之后建议按以下顺序推进：

1. YOLO-only detect-only 改造：停止调用 `model.track(...)`，不再生成 track ID。
2. 删除追踪相关字段和 UI：移除 `trackingObjects`、`objectId`、轨迹查看、轨迹合并配置。
3. 稳定 Windows `.pt + Ultralytics/PyTorch` 闭环：用 golden samples 验收事件输出。
4. 再评估 Windows ONNX Runtime：单独冻结 `best.onnx`、CUDA EP、runtime 包布局和 `.pt/.onnx` 对齐。
5. 最后评估预处理 GPU 加速：OpenCV CUDA、C++ OpenCV、NVDEC/NVENC 均不进入前期改造。

## 后续实现影响面

Phase 0 之后的 YOLO-only 实现会影响以下区域：

- Python worker：`ai-processor/desktop_worker.py`
- YOLO detector：替换当前 `ai-processor/analyzer/yolo_tracker.py` 中的 tracking 调用
- 视频主流程：`ai-processor/analyzer/video_processor.py`
- 事件生成：替换当前基于追踪对象的 `anomaly_event_generator.py`
- Rust worker payload：`frontend/src-tauri/src/domain/worker.rs`
- Rust DB schema：`frontend/src-tauri/src/db/schema.rs`
- Rust 结果持久化：`frontend/src-tauri/src/db/result_repo.rs`
- 前端 API 类型：`frontend/app/composables/useTaskApi.ts`
- 前端首页配置：`frontend/app/pages/index.vue`
- 前端任务详情：`frontend/app/pages/tasks/[id].vue`
- 视频时间轴：`frontend/app/components/VideoPlayer.vue`
- 报告展示与导出：`frontend/app/components/ReportPreview.vue`、`frontend/app/composables/useReportGenerator.ts`

ONNX 与 Windows runtime 包布局改造后续单独建文档或章节处理。

## 当前冻结结论

Phase 0 冻结如下：

- 先做 YOLO-only，不先做 ONNX。
- 缺陷事件为六类 YOLO-only 事件。
- Windows 使用 0-based frame index。
- 动态指标暂不改动。
- golden sample 以帧号为主进行验收。
- ONNX Runtime 和预处理 GPU 加速都后置优化。
