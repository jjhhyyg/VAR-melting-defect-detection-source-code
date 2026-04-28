# VAR 熔池分析桌面端源码仓库

简体中文 | [English](README.md)

> 当前仓库采用 Tauri 桌面端形态：Nuxt/Tauri 前端负责本地任务管理，Python worker 通过本地 job 文件和 stdout NDJSON 事件负责本地视频分析。

## 项目概览

仓库仍采用主仓库 + Git submodule 组织方式，但只保留当前桌面端需要的两个子模块：

- `frontend`：Nuxt 4 + Tauri 2 桌面端应用
- `ai-processor`：桌面 worker、结果汇总与 C++ ONNX GPU sidecar 调用链路

当前主链路：

1. 用户在桌面端批量导入本地视频
2. Tauri/Rust 核心写入本地 SQLite 任务库并维护 FIFO 队列
3. 调度器按最大并发数和资源阈值启动本地 worker
4. Python worker 调用 GPU 预处理 sidecar 和 C++ ONNX CUDA analyzer，通过 stdout NDJSON 上报进度、结果和文件路径
5. 桌面端更新任务表格、详情页、`detections.json` 前端 overlay 和报告数据

## 仓库结构

```text
codes/
├── frontend/       # Git submodule：Nuxt + Tauri 桌面端
├── ai-processor/   # Git submodule：桌面 worker + C++ ONNX GPU 分析链路
├── gpu-analyzer/   # C++ GPU 预处理和 ONNX CUDA 分析 sidecar
├── docs/           # macOS 发布指南、功能验证清单等文档
├── .codex/skills/  # 项目级 Codex skills
└── AGENTS.md       # 项目协作规则
```

## 正确拉取方式

```bash
git clone --recurse-submodules https://github.com/jjhhyyg/VAR-melting-defect-detection-source-code.git
cd VAR-melting-defect-detection-source-code
```

如果已经忘记拉 submodule：

```bash
git submodule update --init --recursive
```

修改 `frontend` 或 `ai-processor` 后，必须先在对应 submodule 内提交，再回到主仓库提交更新后的 submodule 指针。

## macOS 桌面开发

```bash
cd frontend
npm install
npm run desktop:dev
```

常用验证：

```bash
cd frontend
npm run typecheck
cargo check --manifest-path src-tauri/Cargo.toml
```

AI worker 基础检查：

```bash
cd ai-processor
python3 -m py_compile desktop_worker.py utils/callback.py analyzer/video_processor.py
python desktop_worker.py --self-check
```

Windows ONNX/GPU 链路自检：

```powershell
cd frontend
npm run desktop:build-gpu-sidecars
src-tauri\resources\runtime\windows-x64\tools\var-gpu-preprocessor.exe --self-check
src-tauri\resources\runtime\windows-x64\tools\var-video-analyzer.exe --self-check-onnx --model src-tauri\resources\models\best.onnx
```

`desktop:build-gpu-sidecars` 还会用 `golden_samples/sample_1.mp4` 执行一次真实 GPU 预处理烟测。GPU 预处理自检应包含 `opencvNvcodecEnabled:true`。

## macOS 发布

所有桌面发布命令都从 `frontend/` 目录运行：

```bash
npm run desktop:macos:ad-hoc
npm run desktop:macos:release-local
npm run desktop:macos:release-public
```

不要把 `tauri dev`、raw `tauri build` 和正式发布脚本混用。完整规则见 [`docs/macOS桌面端发布指南.md`](docs/macOS桌面端发布指南.md)。

## Windows 发布

Windows 采用双包分发：

- `VAR Desktop_<version>_x64-setup.exe`：主程序 NSIS 安装包，不内置 CUDA worker/runtime
- `VAR-Desktop-CUDA-Runtime-windows-x64-<version>.zip`：CUDA 算法运行时包

构建顺序：

```powershell
cd frontend
npm run desktop:windows:runtime
npm run desktop:windows:build
```

`desktop:windows:runtime` 会依次执行 ONNX 导出、GPU sidecar 构建、Python worker 打包和 runtime zip 生成。导出、构建和运行时都要求 NVIDIA GPU/CUDA 链路可用；当前不提供 CPU fallback。

Windows 首次启动或 App 版本与 runtime build id 不一致时，会强制要求导入匹配的算法包 zip，否则不能进入主程序。完整规则见 [`docs/Windows桌面端发布指南.md`](docs/Windows桌面端发布指南.md)。

## 文档导航

- [`docs/桌面端完整功能验证清单.md`](docs/桌面端完整功能验证清单.md)
- [`docs/macOS桌面端发布指南.md`](docs/macOS桌面端发布指南.md)
- [`docs/Windows桌面端发布指南.md`](docs/Windows桌面端发布指南.md)
- [`docs/instruction.md`](docs/instruction.md)
- [`docs/视频分析原生化重构需求澄清.md`](docs/视频分析原生化重构需求澄清.md)
- [`docs/Windows_C++_GPU_Analyzer_方案C.md`](docs/Windows_C++_GPU_Analyzer_方案C.md)
- [`frontend/README.zh.md`](frontend/README.zh.md)
- [`ai-processor/README.zh.md`](ai-processor/README.zh.md)

## 许可证

本项目采用 GNU Affero General Public License v3.0 (AGPL-3.0) 许可证。
