# Windows 桌面端发布指南

本文档只覆盖 Windows 本地打包与双包分发。Windows 签名、CI/CD、自动更新暂不纳入当前流程。

## 1. 分发产物

Windows 发布包含两个文件：

- `VAR Desktop_0.1.0_x64-setup.exe`：主程序 NSIS 安装包
- `VAR-Desktop-CUDA-Runtime-windows-x64-0.1.0.zip`：CUDA 算法运行时包

主安装包不内置完整 CUDA worker/runtime。首次启动或 App 版本与 runtime build id 不一致时，应用会强制要求导入匹配的算法包 zip。

## 2. 本地环境

推荐使用 conda 环境 `var-env`：

```powershell
conda create -n var-env -y python=3.12
conda install -n var-env -y pytorch torchvision pytorch-cuda=12.4 -c pytorch -c nvidia
conda install -n var-env -y ffmpeg
conda run -n var-env python -m pip install -r ai-processor/requirements-desktop-windows-cuda.txt "pyinstaller>=6.0.0"
```

模型文件必须存在：

```text
ai-processor/weights/best.pt
```

该文件已被 `ai-processor/.gitignore` 忽略，不应提交到 Git。

## 3. 构建顺序

所有桌面命令都从 `frontend/` 目录运行：

```powershell
cd frontend
npm run desktop:windows:runtime
npm run desktop:windows:build
```

`desktop:windows:runtime` 会先运行 `desktop:build-worker`，再生成 runtime zip。`desktop:windows:build` 使用 `src-tauri/tauri.windows.conf.json`，只构建主程序 NSIS 安装包，避免把 5GB+ CUDA runtime 打进 installer。

## 4. 安装与首次启动

1. 安装 `VAR Desktop_0.1.0_x64-setup.exe`。
2. 启动 VAR Desktop。
3. 如果提示导入算法包，选择 `VAR-Desktop-CUDA-Runtime-windows-x64-0.1.0.zip`。
4. 应用会校验 `runtime-manifest.json`、平台、版本、文件 hash，并运行：
   - `desktop_worker.exe --self-check`
   - `ffmpeg.exe -version`
   - `ffprobe.exe -version`
5. 自检通过后进入主程序。

## 5. 升级规则

当前版本采用严格绑定：

```text
requiredRuntimeBuildId = App version
```

升级主程序后，如果本机 runtime build id 与新 App 版本不一致，应用会再次强制要求导入新的算法包 zip。新 runtime 自检通过并切换成功后，旧 runtime 会被删除，不保留回滚版本。

## 6. macOS 边界

Windows 双包流程不要混入 macOS 发布。macOS 仍使用：

```bash
npm run desktop:macos:ad-hoc
npm run desktop:macos:release-local
npm run desktop:macos:release-public
```

macOS 发布入口仍是 `scripts/macos-release.mjs`，负责签名、notarization、staple 和 Gatekeeper 校验。
