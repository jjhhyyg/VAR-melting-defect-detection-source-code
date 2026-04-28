cmake_minimum_required(VERSION 3.25)

project(var_gpu_preprocessor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED VIDEO_CODEC_SDK_ROOT)
  set(VIDEO_CODEC_SDK_ROOT "C:/Video_Codec_SDK_13.0.37" CACHE PATH "NVIDIA Video Codec SDK root")
endif()

if(NOT DEFINED ONNXRUNTIME_ROOT)
  set(ONNXRUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.deps/onnxruntime-gpu-windows-1.25.0" CACHE PATH "ONNX Runtime GPU Windows NuGet package root")
endif()

find_package(OpenCV REQUIRED COMPONENTS world)
find_package(CUDAToolkit REQUIRED)

add_executable(var-gpu-preprocessor
  src/main.cpp
)

target_include_directories(var-gpu-preprocessor PRIVATE
  "${VIDEO_CODEC_SDK_ROOT}/Interface"
)

target_link_directories(var-gpu-preprocessor PRIVATE
  "${VIDEO_CODEC_SDK_ROOT}/Lib/win/x64"
)

target_link_libraries(var-gpu-preprocessor PRIVATE
  ${OpenCV_LIBS}
  CUDA::cudart
  nvcuvid
  nvencodeapi
)

if(MSVC)
  target_compile_options(var-gpu-preprocessor PRIVATE /W4 /permissive- /EHsc)
  target_compile_definitions(var-gpu-preprocessor PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

add_executable(var-video-analyzer
  src/video_analyzer_main.cpp
)

target_include_directories(var-video-analyzer PRIVATE
  "${ONNXRUNTIME_ROOT}/buildTransitive/native/include"
)

target_link_directories(var-video-analyzer PRIVATE
  "${ONNXRUNTIME_ROOT}/runtimes/win-x64/native"
)

target_link_libraries(var-video-analyzer PRIVATE
  ${OpenCV_LIBS}
  onnxruntime
)

if(MSVC)
  target_compile_options(var-video-analyzer PRIVATE /W4 /permissive- /EHsc)
  target_compile_definitions(var-video-analyzer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
