# VAR 熔池分析 macOS 桌面端源码仓库

简体中文 | [English](README.md)

> 当前仓库已经收敛为 macOS Tauri 桌面端形态：Nuxt/Tauri 前端负责本地任务管理，Python worker 负责本地视频分析，不再保留旧的 Java 后端、Docker Compose、RabbitMQ/Redis/PostgreSQL 部署架构。

## 项目概览

仓库仍采用主仓库 + Git submodule 组织方式，但只保留当前桌面端需要的两个子模块：

- `frontend`：Nuxt 4 + Tauri 2 桌面端应用
- `ai-processor`：桌面 worker 与 YOLO 分析链路

当前主链路：

1. 用户在 macOS 桌面端批量导入本地视频
2. Tauri/Rust 核心写入本地 SQLite 任务库并维护 FIFO 队列
3. 调度器按最大并发数和 macOS 资源阈值启动本地 worker
4. Python worker 通过 stdout NDJSON 上报进度、结果和文件路径
5. 桌面端更新任务表格、详情页、结果视频和报告数据

## 仓库结构

```text
codes/
├── frontend/       # Git submodule：Nuxt + Tauri 桌面端
├── ai-processor/   # Git submodule：桌面 worker + YOLO 分析链路
├── docs/           # macOS 发布指南、功能验证清单等文档
├── storage/        # 本地历史测试数据和媒体文件
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

## macOS 发布

所有桌面发布命令都从 `frontend/` 目录运行：

```bash
npm run desktop:macos:ad-hoc
npm run desktop:macos:release-local
npm run desktop:macos:release-public
```

不要把 `tauri dev`、raw `tauri build` 和正式发布脚本混用。完整规则见 [`docs/macOS桌面端发布指南.md`](docs/macOS桌面端发布指南.md)。

## 文档导航

- [`docs/桌面端完整功能验证清单.md`](docs/桌面端完整功能验证清单.md)
- [`docs/macOS桌面端发布指南.md`](docs/macOS桌面端发布指南.md)
- [`frontend/README.zh.md`](frontend/README.zh.md)
- [`ai-processor/README.zh.md`](ai-processor/README.zh.md)

## 许可证

本项目采用 GNU Affero General Public License v3.0 (AGPL-3.0) 许可证。
