# VAR Molten Pool Analysis Desktop Source Code

[简体中文](README.zh.md) | English

> This repository targets the Tauri desktop architecture. The Nuxt/Tauri app manages local tasks, while the Python worker performs local video analysis through local job files and stdout NDJSON events.

## Overview

The repository still uses a main repository plus Git submodules, but only the desktop-relevant modules remain:

- `frontend`: Nuxt 4 + Tauri 2 desktop app
- `ai-processor`: desktop worker, result aggregation, and C++ ONNX GPU sidecar integration

Current flow:

1. The user imports one or more local videos in the desktop UI.
2. The Tauri/Rust core writes tasks to local SQLite and maintains a FIFO queue.
3. The scheduler starts local workers according to max concurrency and resource limits.
4. The Python worker calls the GPU preprocessing sidecar and the C++ ONNX CUDA analyzer, then reports progress, result data, and file paths through stdout NDJSON events.
5. The desktop app updates the task table, detail page, `detections.json` frontend overlay, and report data.

## Repository Structure

```text
codes/
├── frontend/       # Git submodule: Nuxt + Tauri desktop app
├── ai-processor/   # Git submodule: desktop worker + C++ ONNX GPU analysis pipeline
├── gpu-analyzer/   # C++ GPU preprocessing and ONNX CUDA analyzer sidecars
├── docs/           # macOS release guide, functional verification checklist
├── .codex/skills/  # Project-level Codex skills
└── AGENTS.md       # Collaboration rules
```

## Clone Correctly

```bash
git clone --recurse-submodules https://github.com/jjhhyyg/VAR-melting-defect-detection-source-code.git
cd VAR-melting-defect-detection-source-code
```

If you already cloned without submodules:

```bash
git submodule update --init --recursive
```

When modifying `frontend` or `ai-processor`, commit inside that submodule first, then commit the updated submodule pointer in the main repository.

## macOS Desktop Development

```bash
cd frontend
npm install
npm run desktop:dev
```

Common checks:

```bash
cd frontend
npm run typecheck
cargo check --manifest-path src-tauri/Cargo.toml
```

AI worker check:

```bash
cd ai-processor
python3 -m py_compile desktop_worker.py utils/callback.py analyzer/video_processor.py
python desktop_worker.py --self-check
```

Windows ONNX/GPU self-check:

```powershell
cd frontend
npm run desktop:build-gpu-sidecars
src-tauri\resources\runtime\windows-x64\tools\var-gpu-preprocessor.exe --self-check
src-tauri\resources\runtime\windows-x64\tools\var-video-analyzer.exe --self-check-onnx --model src-tauri\resources\models\best.onnx
```

`desktop:build-gpu-sidecars` also runs a real GPU preprocessing smoke test with `golden_samples/sample_1.mp4`. The GPU preprocessor self-check should include `opencvNvcodecEnabled:true`.

## macOS Release

All desktop release commands must be run from `frontend/`:

```bash
npm run desktop:macos:ad-hoc
npm run desktop:macos:release-local
npm run desktop:macos:release-public
```

Do not mix `tauri dev`, raw `tauri build`, and the formal release scripts. See [`docs/macOS桌面端发布指南.md`](docs/macOS桌面端发布指南.md).

## Windows Release

Windows distribution uses two artifacts:

- `VAR Desktop_<version>_x64-setup.exe`: the lightweight NSIS installer for the main app
- `VAR-Desktop-CUDA-Runtime-windows-x64-<version>.zip`: the CUDA analysis runtime package

Build the runtime package first, then build the installer:

```powershell
cd frontend
npm run desktop:windows:runtime
npm run desktop:windows:build
```

`desktop:windows:runtime` runs ONNX export, GPU sidecar build, Python worker packaging, and runtime zip generation in that order. Export, build, and runtime all require the NVIDIA GPU/CUDA path; there is no CPU fallback in the current Windows CUDA flow.

On first launch, or whenever the app version and runtime build id differ, the Windows app blocks the main UI until the matching runtime zip is imported. See [`docs/Windows桌面端发布指南.md`](docs/Windows桌面端发布指南.md).

## Documentation

- [`docs/桌面端完整功能验证清单.md`](docs/桌面端完整功能验证清单.md)
- [`docs/macOS桌面端发布指南.md`](docs/macOS桌面端发布指南.md)
- [`docs/Windows桌面端发布指南.md`](docs/Windows桌面端发布指南.md)
- [`docs/instruction.md`](docs/instruction.md)
- [`docs/视频分析原生化重构需求澄清.md`](docs/视频分析原生化重构需求澄清.md)
- [`docs/Windows_C++_GPU_Analyzer_方案C.md`](docs/Windows_C++_GPU_Analyzer_方案C.md)
- [`frontend/README.md`](frontend/README.md)
- [`ai-processor/README.md`](ai-processor/README.md)

## License

This project is licensed under the GNU Affero General Public License v3.0 (AGPL-3.0).
