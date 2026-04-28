#include <cuda_runtime_api.h>
#include <nvEncodeAPI.h>
#include <opencv2/core.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/cvconfig.h>
#include <opencv2/cudacodec.hpp>
#include <opencv2/videoio.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Options {
    bool selfCheck = false;
    std::string inputPath;
    std::string outputPath;
    double fps = 0.0;
};

struct Timer {
    std::chrono::steady_clock::time_point startedAt = std::chrono::steady_clock::now();

    double elapsedSeconds() const {
        const auto elapsed = std::chrono::steady_clock::now() - startedAt;
        return std::chrono::duration<double>(elapsed).count();
    }

    void reset() {
        startedAt = std::chrono::steady_clock::now();
    }
};

std::string jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            out << ch;
            break;
        }
    }
    return out.str();
}

void emitRaw(const std::string& json) {
    std::cout << json << '\n';
    std::cout.flush();
}

void emitError(const std::string& code, const std::string& message) {
    emitRaw("{\"type\":\"error\",\"code\":\"" + jsonEscape(code) + "\",\"message\":\"" + jsonEscape(message) + "\"}");
}

void emitProgress(std::uint64_t currentFrame, std::uint64_t totalFrames, double elapsedSeconds) {
    std::ostringstream out;
    out << "{\"type\":\"progress\",\"status\":\"PREPROCESSING\",\"currentFrame\":" << currentFrame
        << ",\"totalFrames\":" << totalFrames
        << ",\"elapsedSeconds\":" << std::fixed << std::setprecision(3) << elapsedSeconds << "}";
    emitRaw(out.str());
}

std::string cudaRuntimeVersionString() {
    int runtimeVersion = 0;
    const cudaError_t err = cudaRuntimeGetVersion(&runtimeVersion);
    if (err != cudaSuccess) {
        return "unknown";
    }

    const int major = runtimeVersion / 1000;
    const int minor = (runtimeVersion % 1000) / 10;
    return std::to_string(major) + "." + std::to_string(minor);
}

std::string cudaDriverVersionString() {
    int driverVersion = 0;
    const cudaError_t err = cudaDriverGetVersion(&driverVersion);
    if (err != cudaSuccess) {
        return "unknown";
    }

    const int major = driverVersion / 1000;
    const int minor = (driverVersion % 1000) / 10;
    return std::to_string(major) + "." + std::to_string(minor);
}

bool canLoadLibrary(const wchar_t* name) {
#ifdef _WIN32
    HMODULE handle = LoadLibraryW(name);
    if (!handle) {
        return false;
    }
    FreeLibrary(handle);
    return true;
#else
    (void)name;
    return false;
#endif
}

std::string nvencApiStatus() {
    uint32_t version = 0;
    const NVENCSTATUS status = NvEncodeAPIGetMaxSupportedVersion(&version);
    if (status != NV_ENC_SUCCESS) {
        return "unavailable";
    }

    const uint32_t major = (version >> 4) & 0x0f;
    const uint32_t minor = version & 0x0f;
    return std::to_string(major) + "." + std::to_string(minor);
}

int runSelfCheck() {
#ifdef HAVE_NVCUVID
    constexpr bool openCvNvcuvidBuildFlag = true;
#else
    constexpr bool openCvNvcuvidBuildFlag = false;
#endif

#ifdef HAVE_NVCUVENC
    constexpr bool openCvNvencBuildFlag = true;
#else
    constexpr bool openCvNvencBuildFlag = false;
#endif

    int cudaDeviceCount = 0;
    const cudaError_t cudaStatus = cudaGetDeviceCount(&cudaDeviceCount);
    const bool cudaAvailable = cudaStatus == cudaSuccess && cudaDeviceCount > 0;

    std::string deviceName;
    if (cudaAvailable) {
        cudaDeviceProp props {};
        if (cudaGetDeviceProperties(&props, 0) == cudaSuccess) {
            deviceName = props.name;
        }
    }

    int openCvCudaDeviceCount = 0;
    try {
        openCvCudaDeviceCount = cv::cuda::getCudaEnabledDeviceCount();
    } catch (const cv::Exception&) {
        openCvCudaDeviceCount = 0;
    }

    const bool nvcuvidLoaded = canLoadLibrary(L"nvcuvid.dll");
    const bool nvencLoaded = canLoadLibrary(L"nvEncodeAPI64.dll");
    const std::string nvencVersion = nvencApiStatus();
    const std::string openCvBuildInformation = cv::getBuildInformation();
    const bool openCvBuildHasNvcuvid = openCvBuildInformation.find("NVCUVID") != std::string::npos;
    const bool openCvBuildHasNvenc = openCvBuildInformation.find("NVCUVENC") != std::string::npos;
    const bool openCvNvcodecEnabled = openCvNvcuvidBuildFlag && openCvNvencBuildFlag && openCvBuildHasNvcuvid && openCvBuildHasNvenc;
    const bool ok = cudaAvailable && openCvCudaDeviceCount > 0 && nvcuvidLoaded && nvencLoaded && openCvNvcodecEnabled;

    std::ostringstream out;
    out << "{\"type\":\"self_check\","
        << "\"ok\":" << (ok ? "true" : "false") << ","
        << "\"opencvVersion\":\"" << jsonEscape(CV_VERSION) << "\","
        << "\"cudaRuntimeVersion\":\"" << jsonEscape(cudaRuntimeVersionString()) << "\","
        << "\"cudaDriverVersion\":\"" << jsonEscape(cudaDriverVersionString()) << "\","
        << "\"cudaDeviceCount\":" << cudaDeviceCount << ","
        << "\"opencvCudaDeviceCount\":" << openCvCudaDeviceCount << ","
        << "\"gpuDeviceName\":\"" << jsonEscape(deviceName) << "\","
        << "\"nvcuvidLoaded\":" << (nvcuvidLoaded ? "true" : "false") << ","
        << "\"nvencLoaded\":" << (nvencLoaded ? "true" : "false") << ","
        << "\"opencvNvcodecEnabled\":" << (openCvNvcodecEnabled ? "true" : "false") << ","
        << "\"opencvNvcuvidBuildFlag\":" << (openCvNvcuvidBuildFlag ? "true" : "false") << ","
        << "\"opencvNvencBuildFlag\":" << (openCvNvencBuildFlag ? "true" : "false") << ","
        << "\"opencvBuildHasNvcuvid\":" << (openCvBuildHasNvcuvid ? "true" : "false") << ","
        << "\"opencvBuildHasNvenc\":" << (openCvBuildHasNvenc ? "true" : "false") << ","
        << "\"nvencApiVersion\":\"" << jsonEscape(nvencVersion) << "\"}";
    emitRaw(out.str());

    return ok ? 0 : 2;
}

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--self-check") {
            options.selfCheck = true;
        } else if (arg == "--input" && i + 1 < argc) {
            options.inputPath = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            options.outputPath = argv[++i];
        } else if (arg == "--fps" && i + 1 < argc) {
            options.fps = std::stod(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            emitRaw("{\"type\":\"usage\",\"usage\":\"var-gpu-preprocessor.exe --self-check | --input input.mp4 --output preprocessed.mp4 [--fps 25]\"}");
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown or incomplete argument: " + arg);
        }
    }
    return options;
}

double readInputFps(const std::string& inputPath, double fallbackFps) {
    if (fallbackFps > 0.0) {
        return fallbackFps;
    }

    cv::VideoCapture cap(inputPath);
    if (!cap.isOpened()) {
        return 25.0;
    }

    const double fps = cap.get(cv::CAP_PROP_FPS);
    return fps > 0.0 ? fps : 25.0;
}

std::uint64_t readInputFrameCount(const std::string& inputPath) {
    cv::VideoCapture cap(inputPath);
    if (!cap.isOpened()) {
        return 0;
    }

    const double frames = cap.get(cv::CAP_PROP_FRAME_COUNT);
    return frames > 0.0 ? static_cast<std::uint64_t>(frames) : 0;
}

int runTranscode(const Options& options) {
    if (options.inputPath.empty() || options.outputPath.empty()) {
        throw std::runtime_error("--input and --output are required");
    }
    if (!std::filesystem::exists(options.inputPath)) {
        throw std::runtime_error("Input video does not exist: " + options.inputPath);
    }

    const int selfCheckStatus = runSelfCheck();
    if (selfCheckStatus != 0) {
        emitError("GPU_SELF_CHECK_FAILED", "GPU preprocessing prerequisites are not available");
        return selfCheckStatus;
    }

    const std::filesystem::path outputPath(options.outputPath);
    if (!outputPath.parent_path().empty()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }

    const double fps = readInputFps(options.inputPath, options.fps);
    const std::uint64_t totalFrames = readInputFrameCount(options.inputPath);

    Timer totalTimer;
    double decodeSeconds = 0.0;
    double encodeSeconds = 0.0;
    std::uint64_t processedFrames = 0;

    cv::Ptr<cv::cudacodec::VideoReader> reader = cv::cudacodec::createVideoReader(options.inputPath);
    if (!reader) {
        throw std::runtime_error("Failed to create cudacodec VideoReader");
    }
    if (!reader->set(cv::cudacodec::ColorFormat::BGR)) {
        throw std::runtime_error("cudacodec VideoReader does not support BGR output");
    }

    cv::cuda::GpuMat frame;
    Timer stageTimer;
    if (!reader->nextFrame(frame)) {
        throw std::runtime_error("Input video contains no decodable frames");
    }
    decodeSeconds += stageTimer.elapsedSeconds();

    cv::cudacodec::EncoderParams encoderParams;
    encoderParams.rateControlMode = cv::cudacodec::ENC_PARAMS_RC_VBR;
    encoderParams.tuningInfo = cv::cudacodec::ENC_TUNING_INFO_HIGH_QUALITY;
    encoderParams.nvPreset = cv::cudacodec::ENC_PRESET_P3;
    encoderParams.encodingProfile = cv::cudacodec::ENC_H264_PROFILE_HIGH;
    encoderParams.gopLength = static_cast<int>(std::max(1.0, fps * 2.0));
    encoderParams.idrPeriod = encoderParams.gopLength;

    cv::Ptr<cv::cudacodec::VideoWriter> writer = cv::cudacodec::createVideoWriter(
        options.outputPath,
        frame.size(),
        cv::cudacodec::Codec::H264,
        fps,
        cv::cudacodec::ColorFormat::BGR,
        encoderParams);
    if (!writer) {
        throw std::runtime_error("Failed to create cudacodec VideoWriter");
    }

    while (true) {
        stageTimer.reset();
        writer->write(frame);
        encodeSeconds += stageTimer.elapsedSeconds();

        ++processedFrames;
        if (processedFrames == 1 || processedFrames % 30 == 0) {
            emitProgress(processedFrames, totalFrames, totalTimer.elapsedSeconds());
        }

        stageTimer.reset();
        if (!reader->nextFrame(frame)) {
            break;
        }
        decodeSeconds += stageTimer.elapsedSeconds();
    }

    stageTimer.reset();
    writer->release();
    encodeSeconds += stageTimer.elapsedSeconds();

    const double totalSeconds = totalTimer.elapsedSeconds();
    const double totalFps = totalSeconds > 0.0 ? static_cast<double>(processedFrames) / totalSeconds : 0.0;

    std::ostringstream out;
    out << "{\"type\":\"result\",\"preprocessingBenchmark\":{"
        << "\"schemaVersion\":2,"
        << "\"backend\":\"opencv-cuda-sidecar\","
        << "\"totalFrames\":" << processedFrames << ","
        << "\"totalDurationSeconds\":" << std::fixed << std::setprecision(3) << totalSeconds << ","
        << "\"totalFps\":" << std::fixed << std::setprecision(3) << totalFps << ","
        << "\"gpuDecodeDurationSeconds\":" << std::fixed << std::setprecision(3) << decodeSeconds << ","
        << "\"gpuProcessingDurationSeconds\":0,"
        << "\"gpuEncodeDurationSeconds\":" << std::fixed << std::setprecision(3) << encodeSeconds << ","
        << "\"gpuSyncDurationSeconds\":0,"
        << "\"otherDurationSeconds\":" << std::fixed << std::setprecision(3)
        << std::max(0.0, totalSeconds - decodeSeconds - encodeSeconds) << ","
        << "\"gpuDeviceName\":\"";

    cudaDeviceProp props {};
    if (cudaGetDeviceProperties(&props, 0) == cudaSuccess) {
        out << jsonEscape(props.name);
    }
    out << "\",\"outputPath\":\"" << jsonEscape(std::filesystem::absolute(options.outputPath).string()) << "\"}}";
    emitRaw(out.str());

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        if (options.selfCheck) {
            return runSelfCheck();
        }
        return runTranscode(options);
    } catch (const cv::Exception& err) {
        emitError("OPENCV_ERROR", err.what());
        return 1;
    } catch (const std::exception& err) {
        emitError("GPU_PREPROCESSOR_ERROR", err.what());
        return 1;
    }
}
