# VAR Molten Pool Analysis macOS Desktop Source Code

[简体中文](README.zh.md) | English

> This repository now targets the macOS Tauri desktop architecture. The Nuxt/Tauri app manages local tasks, while the Python worker performs local video analysis. The legacy Java backend, Docker Compose, RabbitMQ, Redis, and PostgreSQL deployment architecture has been removed.

## Overview

The repository still uses a main repository plus Git submodules, but only the desktop-relevant modules remain:

- `frontend`: Nuxt 4 + Tauri 2 desktop app
- `ai-processor`: desktop worker and YOLO analysis pipeline

Current flow:

1. The user imports one or more local videos in the macOS desktop UI.
2. The Tauri/Rust core writes tasks to local SQLite and maintains a FIFO queue.
3. The scheduler starts local workers according to max concurrency and macOS resource limits.
4. The Python worker reports progress, result data, and file paths through stdout NDJSON events.
5. The desktop app updates the task table, detail page, result videos, and report data.

## Repository Structure

```text
codes/
├── frontend/       # Git submodule: Nuxt + Tauri desktop app
├── ai-processor/   # Git submodule: desktop worker + YOLO analysis pipeline
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

## macOS Release

All desktop release commands must be run from `frontend/`:

```bash
npm run desktop:macos:ad-hoc
npm run desktop:macos:release-local
npm run desktop:macos:release-public
```

Do not mix `tauri dev`, raw `tauri build`, and the formal release scripts. See [`docs/macOS桌面端发布指南.md`](docs/macOS桌面端发布指南.md).

## Documentation

- [`docs/桌面端完整功能验证清单.md`](docs/桌面端完整功能验证清单.md)
- [`docs/macOS桌面端发布指南.md`](docs/macOS桌面端发布指南.md)
- [`frontend/README.md`](frontend/README.md)
- [`ai-processor/README.md`](ai-processor/README.md)

## License

This project is licensed under the GNU Affero General Public License v3.0 (AGPL-3.0).
