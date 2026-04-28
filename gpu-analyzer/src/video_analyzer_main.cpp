#include <onnxruntime_cxx_api.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr std::array<const char*, 6> kClassNames {
    "\xE7\x86\x94\xE6\xB1\xA0\xE6\x9C\xAA\xE5\x88\xB0\xE8\xBE\xB9",
    "\xE7\x94\xB5\xE6\x9E\x81\xE7\xB2\x98\xE8\xBF\x9E\xE7\x89\xA9",
    "\xE9\x94\xAD\xE5\x86\xA0",
    "\xE8\xBE\x89\xE5\x85\x89",
    "\xE8\xBE\xB9\xE5\xBC\xA7\xEF\xBC\x88\xE4\xBE\xA7\xE5\xBC\xA7\xEF\xBC\x89",
    "\xE7\x88\xAC\xE5\xBC\xA7",
};

struct Options {
    bool selfCheckOnnx = false;
    std::string modelPath;
    std::string inputPath;
    std::string outputDetectionsPath;
    float confThreshold = 0.5F;
    float iouThreshold = 0.45F;
    int progressInterval = 30;
};

struct Detection {
    int frameIndex = 0;
    int classId = 0;
    float confidence = 0.0F;
    float x1 = 0.0F;
    float y1 = 0.0F;
    float x2 = 0.0F;
    float y2 = 0.0F;
};

struct LetterboxResult {
    cv::Mat image;
    float scale = 1.0F;
    float padX = 0.0F;
    float padY = 0.0F;
};

struct Timer {
    std::chrono::steady_clock::time_point startedAt = std::chrono::steady_clock::now();

    double elapsedSeconds() const {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - startedAt).count();
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

void emitProgress(int currentFrame, int totalFrames, double elapsedSeconds) {
    std::ostringstream out;
    out << "{\"type\":\"progress\",\"status\":\"ANALYZING\",\"currentFrame\":" << currentFrame
        << ",\"totalFrames\":" << totalFrames
        << ",\"elapsedSeconds\":" << std::fixed << std::setprecision(3) << elapsedSeconds << "}";
    emitRaw(out.str());
}

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return L"";
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        throw std::runtime_error("Failed to convert UTF-8 path to UTF-16");
    }
    std::wstring wide(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide.data(), size);
    return wide;
}
#endif

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--self-check-onnx") {
            options.selfCheckOnnx = true;
        } else if (arg == "--model" && i + 1 < argc) {
            options.modelPath = argv[++i];
        } else if (arg == "--input" && i + 1 < argc) {
            options.inputPath = argv[++i];
        } else if (arg == "--output-detections" && i + 1 < argc) {
            options.outputDetectionsPath = argv[++i];
        } else if (arg == "--conf" && i + 1 < argc) {
            options.confThreshold = std::stof(argv[++i]);
        } else if (arg == "--iou" && i + 1 < argc) {
            options.iouThreshold = std::stof(argv[++i]);
        } else if (arg == "--progress-interval" && i + 1 < argc) {
            options.progressInterval = std::max(1, std::stoi(argv[++i]));
        } else if (arg == "--help" || arg == "-h") {
            emitRaw("{\"type\":\"usage\",\"usage\":\"var-video-analyzer.exe --self-check-onnx --model best.onnx | --input input.mp4 --model best.onnx --output-detections detections.json [--conf 0.5] [--iou 0.45]\"}");
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown or incomplete argument: " + arg);
        }
    }
    return options;
}

std::string shapeToJson(const std::vector<int64_t>& shape) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < shape.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << shape[i];
    }
    out << "]";
    return out.str();
}

std::vector<int64_t> resolveStaticInputShape(const std::vector<int64_t>& inputShape) {
    if (inputShape.size() != 4) {
        throw std::runtime_error("ONNX input must be NCHW");
    }

    std::vector<int64_t> resolved = inputShape;
    const int64_t fallback[] = {1, 3, 640, 1024};
    for (std::size_t i = 0; i < resolved.size(); ++i) {
        if (resolved[i] <= 0) {
            resolved[i] = fallback[i];
        }
    }
    return resolved;
}

Ort::Session createCudaSession(Ort::Env& env, const std::string& modelPath) {
    Ort::SessionOptions sessionOptions;
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    Ort::CUDAProviderOptions cudaOptions;
    sessionOptions.AppendExecutionProvider_CUDA_V2(*cudaOptions);

#ifdef _WIN32
    const std::wstring wideModelPath = utf8ToWide(modelPath);
    return Ort::Session(env, wideModelPath.c_str(), sessionOptions);
#else
    return Ort::Session(env, modelPath.c_str(), sessionOptions);
#endif
}

LetterboxResult letterbox(const cv::Mat& frame, int targetHeight, int targetWidth) {
    const int originalHeight = frame.rows;
    const int originalWidth = frame.cols;
    const float scale = std::min(
        static_cast<float>(targetHeight) / static_cast<float>(originalHeight),
        static_cast<float>(targetWidth) / static_cast<float>(originalWidth));

    const int resizedWidth = static_cast<int>(std::round(static_cast<float>(originalWidth) * scale));
    const int resizedHeight = static_cast<int>(std::round(static_cast<float>(originalHeight) * scale));
    const int padWidth = targetWidth - resizedWidth;
    const int padHeight = targetHeight - resizedHeight;
    const float halfPadWidth = static_cast<float>(padWidth) / 2.0F;
    const float halfPadHeight = static_cast<float>(padHeight) / 2.0F;

    cv::Mat resized;
    if (originalWidth != resizedWidth || originalHeight != resizedHeight) {
        cv::resize(frame, resized, cv::Size(resizedWidth, resizedHeight), 0.0, 0.0, cv::INTER_LINEAR);
    } else {
        resized = frame;
    }

    const int top = static_cast<int>(std::round(halfPadHeight - 0.1F));
    const int bottom = static_cast<int>(std::round(halfPadHeight + 0.1F));
    const int left = static_cast<int>(std::round(halfPadWidth - 0.1F));
    const int right = static_cast<int>(std::round(halfPadWidth + 0.1F));

    cv::Mat padded;
    cv::copyMakeBorder(resized, padded, top, bottom, left, right, cv::BORDER_CONSTANT, cv::Scalar(114, 114, 114));
    return LetterboxResult {padded, scale, static_cast<float>(left), static_cast<float>(top)};
}

std::vector<float> frameToTensor(const cv::Mat& frame, int inputHeight, int inputWidth) {
    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);

    std::vector<float> tensor(static_cast<std::size_t>(3 * inputHeight * inputWidth));
    const int planeSize = inputHeight * inputWidth;
    for (int y = 0; y < inputHeight; ++y) {
        const cv::Vec3b* row = rgb.ptr<cv::Vec3b>(y);
        for (int x = 0; x < inputWidth; ++x) {
            const int offset = y * inputWidth + x;
            tensor[static_cast<std::size_t>(offset)] = static_cast<float>(row[x][0]) / 255.0F;
            tensor[static_cast<std::size_t>(planeSize + offset)] = static_cast<float>(row[x][1]) / 255.0F;
            tensor[static_cast<std::size_t>(2 * planeSize + offset)] = static_cast<float>(row[x][2]) / 255.0F;
        }
    }
    return tensor;
}

float boxIou(const Detection& left, const Detection& right) {
    const float x1 = std::max(left.x1, right.x1);
    const float y1 = std::max(left.y1, right.y1);
    const float x2 = std::min(left.x2, right.x2);
    const float y2 = std::min(left.y2, right.y2);
    const float intersection = std::max(0.0F, x2 - x1) * std::max(0.0F, y2 - y1);
    const float leftArea = std::max(0.0F, left.x2 - left.x1) * std::max(0.0F, left.y2 - left.y1);
    const float rightArea = std::max(0.0F, right.x2 - right.x1) * std::max(0.0F, right.y2 - right.y1);
    const float unionArea = leftArea + rightArea - intersection;
    return unionArea > 0.0F ? intersection / unionArea : 0.0F;
}

std::vector<Detection> classAwareNms(std::vector<Detection> detections, float iouThreshold, int maxDet = 300) {
    std::vector<Detection> selected;
    for (int classId = 0; classId < static_cast<int>(kClassNames.size()); ++classId) {
        std::vector<Detection> classDetections;
        std::copy_if(
            detections.begin(),
            detections.end(),
            std::back_inserter(classDetections),
            [classId](const Detection& detection) { return detection.classId == classId; });

        std::sort(classDetections.begin(), classDetections.end(), [](const Detection& left, const Detection& right) {
            return left.confidence > right.confidence;
        });

        while (!classDetections.empty()) {
            Detection current = classDetections.front();
            selected.push_back(current);
            if (static_cast<int>(selected.size()) >= maxDet) {
                break;
            }

            std::vector<Detection> remaining;
            for (std::size_t i = 1; i < classDetections.size(); ++i) {
                if (boxIou(current, classDetections[i]) <= iouThreshold) {
                    remaining.push_back(classDetections[i]);
                }
            }
            classDetections = std::move(remaining);
        }
        if (static_cast<int>(selected.size()) >= maxDet) {
            break;
        }
    }

    std::sort(selected.begin(), selected.end(), [](const Detection& left, const Detection& right) {
        return left.confidence > right.confidence;
    });
    if (static_cast<int>(selected.size()) > maxDet) {
        selected.resize(static_cast<std::size_t>(maxDet));
    }
    return selected;
}

class CppOnnxDetector {
public:
    explicit CppOnnxDetector(const std::string& modelPath)
        : env_(ORT_LOGGING_LEVEL_WARNING, "var-video-analyzer"),
          session_(createCudaSession(env_, modelPath)) {
        Ort::AllocatorWithDefaultOptions allocator;
        auto inputNameAllocated = session_.GetInputNameAllocated(0, allocator);
        auto outputNameAllocated = session_.GetOutputNameAllocated(0, allocator);
        inputName_ = inputNameAllocated.get();
        outputName_ = outputNameAllocated.get();

        const auto inputInfo = session_.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo();
        inputShape_ = resolveStaticInputShape(inputInfo.GetShape());
        inputHeight_ = static_cast<int>(inputShape_[2]);
        inputWidth_ = static_cast<int>(inputShape_[3]);

        const auto outputInfo = session_.GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo();
        outputShape_ = outputInfo.GetShape();
    }

    const std::string& inputName() const {
        return inputName_;
    }

    const std::string& outputName() const {
        return outputName_;
    }

    const std::vector<int64_t>& inputShape() const {
        return inputShape_;
    }

    const std::vector<int64_t>& outputShape() const {
        return outputShape_;
    }

    std::vector<Detection> detectFrame(const cv::Mat& frame, int frameIndex, float confThreshold, float iouThreshold) {
        LetterboxResult boxed = letterbox(frame, inputHeight_, inputWidth_);
        std::vector<float> tensorData = frameToTensor(boxed.image, inputHeight_, inputWidth_);

        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            tensorData.data(),
            tensorData.size(),
            inputShape_.data(),
            inputShape_.size());

        const std::array<const char*, 1> inputNames {inputName_.c_str()};
        const std::array<const char*, 1> outputNames {outputName_.c_str()};
        std::vector<Ort::Value> outputs = session_.Run(
            Ort::RunOptions {nullptr},
            inputNames.data(),
            &inputTensor,
            inputNames.size(),
            outputNames.data(),
            outputNames.size());

        if (outputs.empty() || !outputs[0].IsTensor()) {
            throw std::runtime_error("ONNX output is empty or not a tensor");
        }

        const float* outputData = outputs[0].GetTensorData<float>();
        const std::vector<int64_t> outputShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        return postprocess(outputData, outputShape, frameIndex, frame.cols, frame.rows, boxed, confThreshold, iouThreshold);
    }

private:
    std::vector<Detection> postprocess(
        const float* outputData,
        const std::vector<int64_t>& outputShape,
        int frameIndex,
        int originalWidth,
        int originalHeight,
        const LetterboxResult& boxed,
        float confThreshold,
        float iouThreshold) const {
        if (outputShape.size() != 3 || outputShape[0] != 1) {
            throw std::runtime_error("Unexpected ONNX output shape: " + shapeToJson(outputShape));
        }

        const int64_t dim1 = outputShape[1];
        const int64_t dim2 = outputShape[2];
        const int expectedRows = 4 + static_cast<int>(kClassNames.size());
        const bool transposed = dim1 == expectedRows;
        const int64_t predictionCount = transposed ? dim2 : dim1;
        const int64_t columns = transposed ? dim1 : dim2;
        if (columns < expectedRows) {
            throw std::runtime_error("Unexpected ONNX output columns: " + shapeToJson(outputShape));
        }

        std::vector<Detection> candidates;
        candidates.reserve(static_cast<std::size_t>(predictionCount));
        for (int64_t row = 0; row < predictionCount; ++row) {
            const auto valueAt = [&](int64_t column) -> float {
                if (transposed) {
                    return outputData[static_cast<std::size_t>(column * predictionCount + row)];
                }
                return outputData[static_cast<std::size_t>(row * columns + column)];
            };

            int classId = 0;
            float confidence = valueAt(4);
            for (int classIndex = 1; classIndex < static_cast<int>(kClassNames.size()); ++classIndex) {
                const float score = valueAt(4 + classIndex);
                if (score > confidence) {
                    confidence = score;
                    classId = classIndex;
                }
            }
            if (confidence < confThreshold) {
                continue;
            }

            const float centerX = valueAt(0);
            const float centerY = valueAt(1);
            const float width = valueAt(2);
            const float height = valueAt(3);
            Detection detection;
            detection.frameIndex = frameIndex;
            detection.classId = classId;
            detection.confidence = confidence;
            detection.x1 = (centerX - width / 2.0F - boxed.padX) / boxed.scale;
            detection.y1 = (centerY - height / 2.0F - boxed.padY) / boxed.scale;
            detection.x2 = (centerX + width / 2.0F - boxed.padX) / boxed.scale;
            detection.y2 = (centerY + height / 2.0F - boxed.padY) / boxed.scale;
            detection.x1 = std::clamp(detection.x1, 0.0F, static_cast<float>(originalWidth));
            detection.x2 = std::clamp(detection.x2, 0.0F, static_cast<float>(originalWidth));
            detection.y1 = std::clamp(detection.y1, 0.0F, static_cast<float>(originalHeight));
            detection.y2 = std::clamp(detection.y2, 0.0F, static_cast<float>(originalHeight));
            candidates.push_back(detection);
        }

        return classAwareNms(std::move(candidates), iouThreshold);
    }

    Ort::Env env_;
    Ort::Session session_;
    std::string inputName_;
    std::string outputName_;
    std::vector<int64_t> inputShape_;
    std::vector<int64_t> outputShape_;
    int inputHeight_ = 0;
    int inputWidth_ = 0;
};

void writeDetectionsJson(const std::string& outputPath, const std::vector<std::vector<Detection>>& allDetections) {
    const std::filesystem::path path(outputPath);
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open detections output: " + outputPath);
    }

    out << "[\n";
    for (std::size_t frameIndex = 0; frameIndex < allDetections.size(); ++frameIndex) {
        if (frameIndex > 0) {
            out << ",\n";
        }
        out << "  [";
        const auto& detections = allDetections[frameIndex];
        for (std::size_t i = 0; i < detections.size(); ++i) {
            const Detection& detection = detections[i];
            if (i > 0) {
                out << ",";
            }
            const float width = detection.x2 - detection.x1;
            const float height = detection.y2 - detection.y1;
            out << "\n    {"
                << "\"frameIndex\":" << detection.frameIndex << ","
                << "\"classId\":" << detection.classId << ","
                << "\"class_id\":" << detection.classId << ","
                << "\"className\":\"" << jsonEscape(kClassNames[static_cast<std::size_t>(detection.classId)]) << "\","
                << "\"class_name\":\"" << jsonEscape(kClassNames[static_cast<std::size_t>(detection.classId)]) << "\","
                << "\"confidence\":" << std::fixed << std::setprecision(6) << detection.confidence << ","
                << "\"box\":[" << detection.x1 << "," << detection.y1 << "," << detection.x2 << "," << detection.y2 << "],"
                << "\"bbox\":[" << detection.x1 << "," << detection.y1 << "," << detection.x2 << "," << detection.y2 << "],"
                << "\"center_x\":" << ((detection.x1 + detection.x2) / 2.0F) << ","
                << "\"center_y\":" << ((detection.y1 + detection.y2) / 2.0F) << ","
                << "\"width\":" << width << ","
                << "\"height\":" << height
                << "}";
        }
        if (!detections.empty()) {
            out << "\n  ";
        }
        out << "]";
    }
    out << "\n]\n";
}

int runSelfCheckOnnx(const std::string& modelPath) {
    if (modelPath.empty()) {
        throw std::runtime_error("--model is required");
    }
    if (!std::filesystem::exists(modelPath)) {
        throw std::runtime_error("ONNX model does not exist: " + modelPath);
    }

    CppOnnxDetector detector(modelPath);

    std::ostringstream out;
    out << "{\"type\":\"onnx_self_check\","
        << "\"ok\":true,"
        << "\"onnxRuntimeVersion\":\"" << jsonEscape(Ort::GetVersionString()) << "\","
        << "\"inputName\":\"" << jsonEscape(detector.inputName()) << "\","
        << "\"outputName\":\"" << jsonEscape(detector.outputName()) << "\","
        << "\"inputShape\":" << shapeToJson(detector.inputShape()) << ","
        << "\"outputShape\":" << shapeToJson(detector.outputShape()) << ","
        << "\"provider\":\"CUDAExecutionProvider\"}";
    emitRaw(out.str());
    return 0;
}

int runVideoDetection(const Options& options) {
    if (options.modelPath.empty() || options.inputPath.empty() || options.outputDetectionsPath.empty()) {
        throw std::runtime_error("--model, --input and --output-detections are required");
    }
    if (!std::filesystem::exists(options.modelPath)) {
        throw std::runtime_error("ONNX model does not exist: " + options.modelPath);
    }
    if (!std::filesystem::exists(options.inputPath)) {
        throw std::runtime_error("Input video does not exist: " + options.inputPath);
    }

    CppOnnxDetector detector(options.modelPath);
    cv::VideoCapture capture(options.inputPath);
    if (!capture.isOpened()) {
        throw std::runtime_error("Failed to open input video: " + options.inputPath);
    }

    const int totalFrames = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_COUNT));
    std::vector<std::vector<Detection>> allDetections;
    if (totalFrames > 0) {
        allDetections.reserve(static_cast<std::size_t>(totalFrames));
    }

    Timer timer;
    int frameIndex = 0;
    cv::Mat frame;
    while (capture.read(frame)) {
        allDetections.push_back(detector.detectFrame(frame, frameIndex, options.confThreshold, options.iouThreshold));
        ++frameIndex;
        if (frameIndex == 1 || frameIndex % options.progressInterval == 0) {
            emitProgress(frameIndex, totalFrames, timer.elapsedSeconds());
        }
    }
    capture.release();

    writeDetectionsJson(options.outputDetectionsPath, allDetections);

    const double totalSeconds = timer.elapsedSeconds();
    const double fps = totalSeconds > 0.0 ? static_cast<double>(frameIndex) / totalSeconds : 0.0;
    std::ostringstream out;
    out << "{\"type\":\"result\","
        << "\"outputDetectionsPath\":\"" << jsonEscape(std::filesystem::absolute(options.outputDetectionsPath).string()) << "\","
        << "\"detectionBenchmark\":{"
        << "\"schemaVersion\":1,"
        << "\"backend\":\"onnxruntime-cuda-cpp\","
        << "\"totalFrames\":" << frameIndex << ","
        << "\"totalDurationSeconds\":" << std::fixed << std::setprecision(3) << totalSeconds << ","
        << "\"totalFps\":" << std::fixed << std::setprecision(3) << fps
        << "}}";
    emitRaw(out.str());
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        if (options.selfCheckOnnx) {
            return runSelfCheckOnnx(options.modelPath);
        }
        if (!options.inputPath.empty() || !options.outputDetectionsPath.empty()) {
            return runVideoDetection(options);
        }
        emitRaw("{\"type\":\"usage\",\"usage\":\"var-video-analyzer.exe --self-check-onnx --model best.onnx | --input input.mp4 --model best.onnx --output-detections detections.json [--conf 0.5] [--iou 0.45]\"}");
        return 2;
    } catch (const Ort::Exception& err) {
        emitError("ONNX_RUNTIME_ERROR", err.what());
        return 1;
    } catch (const cv::Exception& err) {
        emitError("OPENCV_ERROR", err.what());
        return 1;
    } catch (const std::exception& err) {
        emitError("VIDEO_ANALYZER_ERROR", err.what());
        return 1;
    }
}
