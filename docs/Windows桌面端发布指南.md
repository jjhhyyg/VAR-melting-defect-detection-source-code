# Windows 桌面端发布指南

本文档只覆盖 Windows 本地打包与双包分发。Windows 签名、CI/CD、自动更新暂不纳入当前流程。

## 1. 分发产物

Windows 发布包含两个文件：

- `VAR Desktop_<version>_x64-setup.exe`：主程序 NSIS 安装包
- `VAR-Desktop-CUDA-Runtime-windows-x64-<version>.zip`：CUDA 算法运行时包

主安装包不内置完整 CUDA worker/runtime。首次启动或 App 版本与 runtime build id 不一致时，应用会强制要求导入匹配的算法包 zip。

## 2. 本地环境

推荐使用 conda 环境 `var-env`：

```powershell
conda create -n var-env -y python=3.12
conda install -n var-env -y pytorch torchvision pytorch-cuda=12.4 -c pytorch -c nvidia
conda install -n var-env -y ffmpeg
conda run -n var-env python -m pip install ultralytics
```

源模型文件必须存在：

```text
ai-processor/weights/best.pt
```

该文件已被 `ai-processor/.gitignore` 忽略，不应提交到 Git。`npm run desktop:windows:runtime` 会先导出 `ai-processor/weights/best.onnx`。导出阶段要求 `torch.cuda.is_available()` 为真；没有 NVIDIA GPU/CUDA 时应显式失败，不走 CPU fallback。

## 3. 构建顺序

所有桌面命令都从 `frontend/` 目录运行：

```powershell
cd frontend
npm run desktop:windows:runtime
npm run desktop:windows:build
```

`desktop:windows:runtime` 的顺序：

1. `npm run desktop:export-onnx`：从 `best.pt` 导出/刷新 `best.onnx`。
2. `npm run desktop:build-gpu-sidecars`：构建 `var-gpu-preprocessor.exe` 和 `var-video-analyzer.exe`，运行 GPU/ONNX self-check，并用 `golden_samples/sample_1.mp4` 执行 GPU 预处理真实视频烟测。
3. `npm run desktop:build-worker`：使用隔离 `.desktop-worker-venv` 打包 Python worker。
4. `scripts/build-windows-runtime-package.mjs`：生成 runtime zip，并写入 `src-tauri/resources/runtime/windows-x64/runtime-package-lock.json`。

`runtime-package-lock.json` 记录 runtime zip 的文件名、大小和 SHA256。`desktop:windows:build` 使用 `src-tauri/tauri.windows.conf.json`，只构建主程序 NSIS 安装包，避免把 2GB+ CUDA runtime 打进 installer，同时把该锁定文件打入主程序。不要反过来构建；否则主程序可能携带旧的算法包 SHA256。

## 4. 安装与首次启动

1. 安装 `VAR Desktop_<version>_x64-setup.exe`。
2. 启动 VAR Desktop。
3. 如果提示导入算法包，选择 `VAR-Desktop-CUDA-Runtime-windows-x64-<version>.zip`。
4. 应用会先校验整个 runtime zip 的 SHA256 是否与主程序内置锁定文件一致，再校验 `runtime-manifest.json`、平台、版本、逐文件 hash，并运行：
   - `desktop_worker.exe --self-check`
   - `ffmpeg.exe -version`
   - `ffprobe.exe -version`
   - NVIDIA 驱动版本和 `nvcuda.dll` / `nvcuvid.dll` / `nvEncodeAPI64.dll` 关键入口点
   - `var-gpu-preprocessor.exe --self-check`
   - `var-video-analyzer.exe --self-check-onnx --model best.onnx`
5. 自检通过后进入主程序。

`var-gpu-preprocessor.exe --self-check` 的成功结果除了 `ok:true` 外，还必须包含 `opencvNvcodecEnabled:true`。该字段用于确认算法包内 OpenCV 真实启用了 NVDEC/NVENC 后端，避免出现导入成功但任务预处理阶段 `throw_no_cuda` 的问题。

## 5. 升级规则

当前版本采用严格绑定：

```text
requiredRuntimeBuildId = App version
runtimePackageSha256 = runtime-package-lock.json 中记录的 SHA256
```

升级主程序后，如果本机 runtime build id 与新 App 版本不一致，应用会再次强制要求导入新的算法包 zip。即使版本号一致，只要 zip 的 SHA256 与主程序锁定文件不一致，也会拒绝导入。新 runtime 自检通过并切换成功后，旧 runtime 会被删除，不保留回滚版本。

## 6. macOS 边界

Windows 双包流程不要混入 macOS 发布。macOS 仍使用：

```bash
npm run desktop:macos:ad-hoc
npm run desktop:macos:release-local
npm run desktop:macos:release-public
```

macOS 发布入口仍是 `scripts/macos-release.mjs`，负责签名、notarization、staple 和 Gatekeeper 校验。
