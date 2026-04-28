# VAR Desktop 开发与打包指引

本文记录 Windows GPU 版本的开发、构建和交付先决条件。所有桌面发布命令默认从 `frontend/` 目录运行。

## 目标运行环境

交付目标是：客户机只需要安装 Windows x64、NVIDIA 驱动和 30/40/50 系 NVIDIA 显卡，不需要额外安装 CUDA Toolkit、OpenCV、VC++ Runtime、FFmpeg 或 Python 环境。

目标机要求：

- NVIDIA RTX 30 / 40 / 50 系显卡。
- `nvidia-smi` 显示的 `CUDA Version` 需要大于等于 `12.9`。
- NVIDIA 驱动需要包含可用的 `nvcuda.dll`、`nvcuvid.dll`、`nvEncodeAPI64.dll`。

算法包会随包携带 CUDA Runtime、cuDNN、NPP、ONNX Runtime、OpenCV、FFmpeg、VC++ Runtime 和 worker 运行文件。NVIDIA 驱动 DLL 不随包携带，必须由目标机驱动提供。

## 本机开发先决条件

本机需要安装：

- Visual Studio 2022 Build Tools / Professional，包含 MSVC x64 工具链。
- CMake。
- Node.js 22+。
- npm。
- Conda 环境 `var-env`，用于导出 ONNX 和构建 Python worker。
- CUDA Toolkit 12.9。
- NVIDIA Video Codec SDK 13.0.37。
- 可用 NVIDIA GPU，构建期 ONNX 导出和 sidecar self-check 会检查 CUDA。

当前固定路径：

```text
CUDA Toolkit:          F:\CUDA\v12.9
NVIDIA Video SDK:      C:\Video_Codec_SDK_13.0.37
OpenCV 源码根目录:     F:\Project\opencv-cpp-gpu
OpenCV 源码:           F:\Project\opencv-cpp-gpu\opencv
OpenCV contrib 源码:   F:\Project\opencv-cpp-gpu\opencv_contrib
OpenCV 通用构建目录:   F:\Project\opencv-cpp-gpu\build-cuda-universal-nvcodec
OpenCV 通用安装目录:   F:\Project\opencv-cpp-gpu\install-cuda-universal-nvcodec
项目根目录:            F:\Project\var-system
前端/桌面目录:         F:\Project\var-system\frontend
GPU sidecar 源码:      F:\Project\var-system\gpu-analyzer
模型源文件:            F:\Project\var-system\ai-processor\weights\best.pt
ONNX 模型:             F:\Project\var-system\ai-processor\weights\best.onnx
```

## OpenCV CUDA 通用版构建

在 `x64 Native Tools Command Prompt for VS 2022` 中运行。注意这是 `cmd`，不是 PowerShell；路径统一用 `/`，避免 CMake 把 `\C` 解析成非法转义。

正式构建前先确认 NVIDIA Video Codec SDK 的 import lib 能被 CMake 找到。OpenCV 的 `cudacodec` 模块即使出现在 `To be built` 中，也不代表 GPU 编解码后端已经启用；必须同时识别到 `NVCUVID` 和 `NVCUVENC`。本机推荐先把 Video Codec SDK 的 import lib 放到 CUDA lib 目录，避免 OpenCV 的旧 FindCUDA 逻辑找不到：

```bat
copy /Y C:\Video_Codec_SDK_13.0.37\Lib\win\x64\nvcuvid.lib F:\CUDA\v12.9\lib\x64\nvcuvid.lib
copy /Y C:\Video_Codec_SDK_13.0.37\Lib\win\x64\nvencodeapi.lib F:\CUDA\v12.9\lib\x64\nvencodeapi.lib
```

```bat
set CUDA_PATH=F:/CUDA/v12.9
set CUDA_PATH_V12_9=F:/CUDA/v12.9
set CUDA_BIN_PATH=F:/CUDA/v12.9

rmdir /s /q F:\Project\opencv-cpp-gpu\build-cuda-universal-nvcodec

cmake -S F:/Project/opencv-cpp-gpu/opencv ^
  -B F:/Project/opencv-cpp-gpu/build-cuda-universal-nvcodec ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -D CMAKE_BUILD_TYPE=Release ^
  -D CMAKE_INSTALL_PREFIX=F:/Project/opencv-cpp-gpu/install-cuda-universal-nvcodec ^
  -D OPENCV_EXTRA_MODULES_PATH=F:/Project/opencv-cpp-gpu/opencv_contrib/modules ^
  -D CUDA_TOOLKIT_ROOT_DIR=F:/CUDA/v12.9 ^
  -D CUDAToolkit_ROOT=F:/CUDA/v12.9 ^
  -D CUDA_BIN_PATH=F:/CUDA/v12.9 ^
  -D VIDEO_CODEC_SDK_ROOT=C:/Video_Codec_SDK_13.0.37 ^
  -D BUILD_SHARED_LIBS=ON ^
  -D BUILD_opencv_world=ON ^
  -D BUILD_LIST=core,imgproc,imgcodecs,highgui,videoio,cudacodec,cudaarithm,cudaimgproc,cudawarping,cudev ^
  -D WITH_CUDA=ON ^
  -D WITH_CUDNN=ON ^
  -D OPENCV_DNN_CUDA=OFF ^
  -D BUILD_opencv_dnn=OFF ^
  -D BUILD_opencv_cudacodec=ON ^
  -D WITH_NVCUVID=ON ^
  -D WITH_NVCUVENC=ON ^
  -D WITH_FFMPEG=ON ^
  -D WITH_OPENCL=OFF ^
  -D WITH_IPP=OFF ^
  -D CUDA_ARCH_BIN="8.6;8.9;12.0" ^
  -D CUDA_ARCH_PTX="12.0" ^
  -D CUDA_FAST_MATH=ON ^
  -D ENABLE_FAST_MATH=ON ^
  -D BUILD_TESTS=OFF ^
  -D BUILD_PERF_TESTS=OFF ^
  -D BUILD_EXAMPLES=OFF ^
  -D BUILD_DOCS=OFF ^
  -D BUILD_JAVA=OFF ^
  -D BUILD_opencv_python3=OFF ^
  -D BUILD_opencv_python_bindings_generator=OFF ^
  -D CMAKE_CXX_FLAGS="/MP" ^
  -D CMAKE_C_FLAGS="/MP"

cmake --build F:/Project/opencv-cpp-gpu/build-cuda-universal-nvcodec --config Release --parallel %NUMBER_OF_PROCESSORS% -- /m
cmake --install F:/Project/opencv-cpp-gpu/build-cuda-universal-nvcodec --config Release
```

加速参数说明：

- `BUILD_LIST` 只编译当前 sidecar 和 `opencv_world` 真实需要的 OpenCV 模块，减少无关模块编译时间。这里保留 `imgcodecs`、`highgui` 是为了满足 `BUILD_opencv_world=ON` 的 CMake 依赖关系，否则配置阶段可能报 `Unknown CMake command "ocv_highgui_configure_target"`。
- 当前缺陷检测推理使用 ONNX Runtime CUDA，不使用 OpenCV DNN，所以 `OPENCV_DNN_CUDA` 和 `BUILD_opencv_dnn` 关闭。
- `/MP`、`--parallel %NUMBER_OF_PROCESSORS%` 和 `/m` 让 MSVC/MSBuild 并行编译。

架构说明：

- `8.6`：RTX 30 系。
- `8.9`：RTX 40 系。
- `12.0`：RTX 50 系。
- `CUDA_ARCH_PTX=12.0`：保留 50 系 PTX，提升后续同代兼容性。

执行上面的 `cmake -S ... -B ...` 配置命令结束后，先检查控制台输出。确认关键行包含 `NVCUVID NVCUVENC` 后，再继续执行 `cmake --build` 和 `cmake --install`：

```text
-- Found NVCUVID: F:/CUDA/v12.9/lib/x64/nvcuvid.lib
-- Found NVCUVENC: ...
NVIDIA CUDA: YES (ver 12.9, CUFFT CUBLAS NVCUVID NVCUVENC FAST_MATH)
```

也可以直接检查生成的 `cvconfig.h`：

```bat
findstr /C:"HAVE_NVCUVID" /C:"HAVE_NVCUVENC" F:\Project\opencv-cpp-gpu\build-cuda-universal-nvcodec\opencv2\cvconfig.h
```

正确结果应包含：

```text
#define HAVE_NVCUVID
#define HAVE_NVCUVENC
```

如果看到 `/* #undef HAVE_NVCUVID */` 或 `/* #undef HAVE_NVCUVENC */`，不要继续安装和打包；这个 OpenCV 会在任务预处理阶段触发 `throw_no_cuda`，表现为 `var-gpu-preprocessor` 报 `The called functionality is disabled for current build or platform`。

安装后可检查：

```bat
F:\Project\opencv-cpp-gpu\install-cuda-universal-nvcodec\x64\vc17\bin\opencv_version.exe --verbose
```

关键输出应包含：

```text
NVIDIA CUDA: YES (ver 12.9, CUFFT CUBLAS NVCUVID NVCUVENC FAST_MATH)
cuDNN: YES
NVIDIA GPU arch: 86 89 120
NVIDIA PTX archs: 120
To be built: ... cudacodec ...
```

注意：`To be built: ... cudacodec ...` 只是必要条件，不是充分条件。实际发布验收以 `NVCUVID NVCUVENC` 和 `HAVE_NVCUVID/HAVE_NVCUVENC` 为准。

## npm 构建指令

所有命令从 `frontend/` 目录运行：

```bat
cd /d F:\Project\var-system\frontend
```

### 导出 ONNX

从 `best.pt` 导出 `best.onnx`：

```bat
npm run desktop:export-onnx
```

默认使用 `ai-processor\weights\best.pt`，输出到 `ai-processor\weights\best.onnx`。导出要求构建机有可用 NVIDIA GPU。

### 构建 GPU sidecar

```bat
npm run desktop:build-gpu-sidecars
```

默认使用：

```text
VAR_OPENCV_ROOT=F:\Project\opencv-cpp-gpu\install-cuda-universal-nvcodec
VAR_CUDA_ROOT=F:\CUDA\v12.9
VAR_VIDEO_CODEC_SDK_ROOT=C:\Video_Codec_SDK_13.0.37
```

如需临时覆盖：

```bat
set VAR_OPENCV_ROOT=F:\Project\opencv-cpp-gpu\install-cuda-universal-nvcodec
set VAR_CUDA_ROOT=F:\CUDA\v12.9
set VAR_VIDEO_CODEC_SDK_ROOT=C:\Video_Codec_SDK_13.0.37
npm run desktop:build-gpu-sidecars
```

该命令会构建并复制：

- `var-gpu-preprocessor.exe`
- `var-video-analyzer.exe`
- OpenCV CUDA DLL
- CUDA/cuDNN/NPP/NVRTC DLL
- ONNX Runtime CUDA DLL
- VC++ Runtime DLL

脚本会执行 sidecar self-check、GPU 预处理真实视频烟测，并扫描 PE 依赖。除 Windows 系统 DLL 和 NVIDIA 驱动 DLL 外，缺少任何 DLL 都会让构建失败。

GPU 预处理烟测默认使用：

```text
F:\Project\var-system\golden_samples\sample_1.mp4
```

该烟测会实际调用 `var-gpu-preprocessor.exe --input ... --output ...`，用于确认 OpenCV `cudacodec` 能创建 GPU 解码器和编码器。单纯 `--self-check` 只能证明 CUDA 设备、NVIDIA 驱动 DLL 和 OpenCV CUDA 基础能力可见，不能完全覆盖 `cudacodec` 的运行链路。

如确需临时跳过烟测：

```bat
set VAR_SKIP_GPU_PREPROCESS_SMOKE=1
npm run desktop:build-gpu-sidecars
```

### 构建 Python worker

```bat
npm run desktop:build-worker
```

该命令会构建桌面 worker，并补齐：

- `desktop_worker.exe`
- `ffmpeg.exe`
- `ffprobe.exe`
- FFmpeg 相关 DLL
- `best.onnx`

### 生成 Windows 算法包

```bat
node scripts\build-windows-runtime-package.mjs
```

输出位置：

```text
frontend\src-tauri\target\release\bundle\runtime\VAR-Desktop-CUDA-Runtime-windows-x64-0.1.6.zip
```

该命令还会生成主程序导入校验用的锁定文件：

```text
frontend\src-tauri\resources\runtime\windows-x64\runtime-package-lock.json
```

锁定文件记录当前算法包 zip 的大小和 SHA256。Windows 主程序安装包会随包携带这个锁定文件，用户导入算法包时会先校验整个 zip 的 SHA256；不匹配则拒绝导入。因此正式发布必须先生成算法包，再构建主程序安装包。

### 一键生成算法包

完整顺序：

```bat
npm run desktop:windows:runtime
```

等价于：

```bat
npm run desktop:export-onnx
npm run desktop:build-gpu-sidecars
npm run desktop:build-worker
node scripts\build-windows-runtime-package.mjs
```

### 构建 Windows 主程序安装包

```bat
npm run desktop:windows:build
```

输出位置：

```text
frontend\src-tauri\target\release\bundle\nsis\VAR Desktop_0.1.6_x64-setup.exe
```

## 推荐发布顺序

```bat
cd /d F:\Project\var-system\frontend

npm run desktop:windows:runtime
npm run desktop:windows:build
```

不要反过来执行。`desktop:windows:runtime` 会生成算法包和 `runtime-package-lock.json`，`desktop:windows:build` 会把该锁定文件打进主程序。这样交付出去的主程序只接受同一批生成的算法包。

交付文件：

```text
VAR Desktop_0.1.6_x64-setup.exe
VAR-Desktop-CUDA-Runtime-windows-x64-0.1.6.zip
```

## 验证命令

### 前端和 Tauri

```bat
npm run lint
npm run typecheck
cd /d F:\Project\var-system\frontend\src-tauri
cargo check
```

### Python worker 编译检查

```bat
cd /d F:\Project\var-system\ai-processor
conda run -n var-env python -m py_compile desktop_worker.py analyzer/video_processor.py analyzer/metrics_calculator.py analyzer/anomaly_event_generator.py utils/callback.py config.py
```

### 隔离 PATH 验证算法包 tools

在 PowerShell 中运行：

```powershell
cd F:\Project\var-system\frontend
$tools=(Resolve-Path 'src-tauri\resources\runtime\windows-x64\tools').Path
$model=(Resolve-Path 'src-tauri\resources\models\best.onnx').Path
$sample=(Resolve-Path '..\golden_samples\sample_1.mp4').Path
$preprocessOutput=Join-Path $env:TEMP 'var-preprocess-check.mp4'
$old=$env:PATH
$env:PATH="$tools;C:\Windows\System32"
try {
  & "$tools\ffmpeg.exe" -version
  & "$tools\ffprobe.exe" -version
  & "$tools\var-gpu-preprocessor.exe" --self-check
  & "$tools\var-gpu-preprocessor.exe" --input $sample --output $preprocessOutput --fps 25.0
  & "$tools\var-video-analyzer.exe" --self-check-onnx --model $model
} finally {
  $env:PATH=$old
}
```

这项验证用于确认算法包不依赖构建机 PATH 中的 CUDA、OpenCV、FFmpeg 或 Conda DLL。

验证成功时应满足：

- `ffmpeg.exe -version` 和 `ffprobe.exe -version` 正常打印版本信息，退出码为 `0`。
- `var-gpu-preprocessor.exe --self-check` 打印一行 JSON，`type` 为 `self_check`，`ok` 为 `true`。
- GPU 预处理自检 JSON 中应包含 CUDA Runtime/Driver 版本、GPU 名称、`cudaDeviceCount` 大于等于 `1`、`opencvCudaDeviceCount` 大于等于 `1`、`nvcuvidLoaded:true`、`nvencLoaded:true`、`opencvNvcodecEnabled:true`、`opencvNvcuvidBuildFlag:true`、`opencvNvencBuildFlag:true`、`opencvBuildHasNvcuvid:true`、`opencvBuildHasNvenc:true`、`nvencApiVersion`。
- `var-gpu-preprocessor.exe --input ... --output ...` 能正常输出 `progress` 和 `result` JSON，退出码为 `0`，并生成临时 `mp4`。这一步用于验证 OpenCV GPU 编解码真实链路。
- `var-video-analyzer.exe --self-check-onnx --model ...` 打印一行 JSON，`type` 为 `onnx_self_check`，`ok` 为 `true`。
- ONNX 自检 JSON 中应包含 `onnxRuntimeVersion`、`inputName`、`outputName`、`inputShape`、`outputShape`，并且 `provider` 为 `CUDAExecutionProvider`。
- 上述命令都不应出现 `0xc0000135`、`0xc0000139`、`DLL load failed`、`The called functionality is disabled for current build or platform` 等错误。

示例关键输出：

```text
{"type":"self_check","ok":true,...,"opencvCudaDeviceCount":1,...,"nvcuvidLoaded":true,"nvencLoaded":true,"opencvNvcodecEnabled":true,...}
{"type":"onnx_self_check","ok":true,...,"provider":"CUDAExecutionProvider"}
```

## 常见问题

### CMake 报 Invalid character escape '\C'

在 `cmd` 中配置 OpenCV 时，CUDA 路径不要写 `F:\CUDA\v12.9`，要写 `F:/CUDA/v12.9`。

### 算法包导入时报 0xc0000135

这是 Windows loader 报“找不到 DLL”。通常说明算法包不完整，或打包脚本漏复制依赖。当前 `build-windows-runtime-package.mjs` 会做 PE 依赖校验，正常情况下不应再出现。

### 算法包导入时报 0xc0000139

这是 Windows loader 报“DLL 入口点不匹配”。常见原因：

- 目标机 NVIDIA 驱动过旧。
- OpenCV / CUDA / NPP / cuDNN DLL 版本混用。
- 算法包不是最新重新生成的完整包。

新版主程序会把导入和 self-check 详情写入 backend log。

日志位置通常为：

```text
%LOCALAPPDATA%\cn.edu.ustb.hyy.var-desktop\logs\backend\<年>\<月>\<日>\backend.log
```

可用 PowerShell 搜索：

```powershell
Get-ChildItem $env:LOCALAPPDATA -Recurse -Filter backend.log -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 5 FullName,LastWriteTime
```
