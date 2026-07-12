set(_gsm_onnxruntime_default_root "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-1.24.4-1.27.1")
if(NOT DEFINED GSM_ONNXRUNTIME_ROOT
   OR GSM_ONNXRUNTIME_ROOT STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-1.24.4")
    set(GSM_ONNXRUNTIME_ROOT "${_gsm_onnxruntime_default_root}" CACHE PATH
        "Path to the unpacked ONNX Runtime SDK (1.24.4 headers/DirectML, 1.27.1 CUDA 13)" FORCE)
else()
    set(GSM_ONNXRUNTIME_ROOT "${GSM_ONNXRUNTIME_ROOT}" CACHE PATH
        "Path to the unpacked ONNX Runtime SDK (1.24.4 headers/DirectML, 1.27.1 CUDA 13)")
endif()

set(_ort_temp_root "${CMAKE_SOURCE_DIR}/tmp/onnxruntime-setup")
set(_ort_package_root "${_ort_temp_root}/packages")
file(MAKE_DIRECTORY "${_ort_package_root}")

function(gsm_download_file url output sha256 description)
    message(STATUS "Downloading ${description}")
    file(DOWNLOAD
        "${url}"
        "${output}"
        EXPECTED_HASH "SHA256=${sha256}"
        SHOW_PROGRESS
        STATUS _download_status
    )
    list(GET _download_status 0 _download_code)
    list(GET _download_status 1 _download_message)
    if(NOT _download_code EQUAL 0)
        message(FATAL_ERROR "Failed to download ${description}: ${_download_message}")
    endif()
endfunction()

if(WIN32)
    set(GSM_WINDOWSML_VERSION "2.1.71")
    set(GSM_WINDOWSML_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/windowsml-${GSM_WINDOWSML_VERSION}" CACHE PATH
        "Path to the unpacked Microsoft.Windows.AI.MachineLearning package")
    if(NOT EXISTS "${GSM_WINDOWSML_ROOT}/runtimes/win-x64/native/onnxruntime.dll")
        set(_windowsml_package "${_ort_package_root}/microsoft.windows.ai.machinelearning.${GSM_WINDOWSML_VERSION}.nupkg")
        gsm_download_file(
            "https://api.nuget.org/v3-flatcontainer/microsoft.windows.ai.machinelearning/${GSM_WINDOWSML_VERSION}/microsoft.windows.ai.machinelearning.${GSM_WINDOWSML_VERSION}.nupkg"
            "${_windowsml_package}"
            "826BA16DD9484AF562D806D3145E5CB4E24B75F28ADF03D718E12F599F004730"
            "Windows ML ${GSM_WINDOWSML_VERSION} package")
        file(REMOVE_RECURSE "${GSM_WINDOWSML_ROOT}")
        file(MAKE_DIRECTORY "${GSM_WINDOWSML_ROOT}")
        file(ARCHIVE_EXTRACT INPUT "${_windowsml_package}" DESTINATION "${GSM_WINDOWSML_ROOT}")
    endif()

    add_library(gsm_windowsml_api INTERFACE)
    target_include_directories(gsm_windowsml_api INTERFACE "${GSM_WINDOWSML_ROOT}/include")
    target_link_libraries(gsm_windowsml_api INTERFACE "${GSM_WINDOWSML_ROOT}/lib/native/x64/Microsoft.Windows.AI.MachineLearning.lib")

    if(NOT EXISTS "${GSM_ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h"
       OR NOT EXISTS "${GSM_ONNXRUNTIME_ROOT}/runtime/cuda/onnxruntime.dll"
       OR NOT EXISTS "${GSM_ONNXRUNTIME_ROOT}/runtime/cuda/onnxruntime_providers_tensorrt.dll"
       OR NOT EXISTS "${GSM_ONNXRUNTIME_ROOT}/runtime/directml/onnxruntime.dll")
        set(_ort_cuda_archive "${_ort_package_root}/onnxruntime-cuda13-1.27.1.zip")
        set(_ort_directml_package "${_ort_package_root}/onnxruntime-directml-1.24.4.nupkg")
        set(_ort_cuda_extract "${_ort_temp_root}/cmake-cuda")
        set(_ort_directml_extract "${_ort_temp_root}/cmake-directml")

        gsm_download_file(
            "https://github.com/microsoft/onnxruntime/releases/download/v1.27.1/onnxruntime-win-x64-gpu_cuda13-1.27.1.zip"
            "${_ort_cuda_archive}"
            "1A88828FB1CA78C9920637ACF0F8D8CE40A854BEF874128C55193E3D1A2B9EF8"
            "ONNX Runtime 1.27.1 CUDA 13 package")
        gsm_download_file(
            "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime.directml/1.24.4/microsoft.ml.onnxruntime.directml.1.24.4.nupkg"
            "${_ort_directml_package}"
            "57E9F11B73437BEF7A309496135D4C1F96B1A8E9DDBA60013FA27BFC1D788681"
            "ONNX Runtime 1.24.4 DirectML package")

        file(REMOVE_RECURSE "${_ort_cuda_extract}" "${_ort_directml_extract}")
        file(MAKE_DIRECTORY "${_ort_cuda_extract}" "${_ort_directml_extract}")
        file(ARCHIVE_EXTRACT INPUT "${_ort_cuda_archive}" DESTINATION "${_ort_cuda_extract}")
        file(ARCHIVE_EXTRACT INPUT "${_ort_directml_package}" DESTINATION "${_ort_directml_extract}")

        file(MAKE_DIRECTORY
            "${GSM_ONNXRUNTIME_ROOT}/include"
            "${GSM_ONNXRUNTIME_ROOT}/runtime/cuda"
            "${GSM_ONNXRUNTIME_ROOT}/runtime/directml")
        file(COPY "${_ort_directml_extract}/build/native/include/"
            DESTINATION "${GSM_ONNXRUNTIME_ROOT}/include")
        file(COPY
            "${_ort_cuda_extract}/onnxruntime-win-x64-gpu_cuda13-1.27.1/lib/onnxruntime.dll"
            "${_ort_cuda_extract}/onnxruntime-win-x64-gpu_cuda13-1.27.1/lib/onnxruntime_providers_cuda.dll"
            "${_ort_cuda_extract}/onnxruntime-win-x64-gpu_cuda13-1.27.1/lib/onnxruntime_providers_shared.dll"
            "${_ort_cuda_extract}/onnxruntime-win-x64-gpu_cuda13-1.27.1/lib/onnxruntime_providers_tensorrt.dll"
            DESTINATION "${GSM_ONNXRUNTIME_ROOT}/runtime/cuda")
        file(GLOB _ort_directml_dlls "${_ort_directml_extract}/runtimes/win-x64/native/*.dll")
        file(COPY ${_ort_directml_dlls}
            DESTINATION "${GSM_ONNXRUNTIME_ROOT}/runtime/directml")
    endif()
elseif(APPLE)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _ort_processor)
    if(NOT _ort_processor MATCHES "^(arm64|aarch64)$")
        message(FATAL_ERROR
            "ONNX Runtime 1.24.4 has no official macOS x64 release asset; "
            "use an arm64 build host or provide GSM_ONNXRUNTIME_ROOT manually.")
    endif()
    if(NOT EXISTS "${GSM_ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h"
       OR NOT EXISTS "${GSM_ONNXRUNTIME_ROOT}/runtime/cpu/libonnxruntime.dylib")
        set(_ort_archive "${_ort_package_root}/onnxruntime-osx-arm64-1.24.4.tgz")
        set(_ort_extract "${_ort_temp_root}/cmake-osx-arm64")
        gsm_download_file(
            "https://github.com/microsoft/onnxruntime/releases/download/v1.24.4/onnxruntime-osx-arm64-1.24.4.tgz"
            "${_ort_archive}"
            "93787795F47E1EEE369182E43ED51B9E5DA0878AB0346AECF4258979B8BBA989"
            "ONNX Runtime 1.24.4 macOS arm64 package")
        file(REMOVE_RECURSE "${_ort_extract}")
        file(MAKE_DIRECTORY "${_ort_extract}" "${GSM_ONNXRUNTIME_ROOT}/runtime/cpu")
        file(ARCHIVE_EXTRACT INPUT "${_ort_archive}" DESTINATION "${_ort_extract}")
        set(_ort_source "${_ort_extract}/onnxruntime-osx-arm64-1.24.4")
        file(COPY "${_ort_source}/include/" DESTINATION "${GSM_ONNXRUNTIME_ROOT}/include")
        file(COPY "${_ort_source}/lib/" DESTINATION "${GSM_ONNXRUNTIME_ROOT}/runtime/cpu"
            FILES_MATCHING PATTERN "libonnxruntime*.dylib")
    endif()
elseif(UNIX)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _ort_processor)
    if(_ort_processor MATCHES "^(x86_64|amd64)$")
        set(_ort_arch "x64")
        set(_ort_sha256 "3A211FBEA252C1E66290658F1B735B772056149F28321E71C308942CDB54B747")
    elseif(_ort_processor MATCHES "^(aarch64|arm64)$")
        set(_ort_arch "aarch64")
        set(_ort_sha256 "866109A9248D057671A039B9D725BE4BD86888E3754140E6701EC621BE9D4D7E")
    else()
        message(FATAL_ERROR "Unsupported Linux architecture for ONNX Runtime: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    if(NOT EXISTS "${GSM_ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h"
       OR NOT EXISTS "${GSM_ONNXRUNTIME_ROOT}/runtime/cpu/libonnxruntime.so")
        set(_ort_archive "${_ort_package_root}/onnxruntime-linux-${_ort_arch}-1.24.4.tgz")
        set(_ort_extract "${_ort_temp_root}/cmake-linux-${_ort_arch}")
        gsm_download_file(
            "https://github.com/microsoft/onnxruntime/releases/download/v1.24.4/onnxruntime-linux-${_ort_arch}-1.24.4.tgz"
            "${_ort_archive}"
            "${_ort_sha256}"
            "ONNX Runtime 1.24.4 Linux ${_ort_arch} package")
        file(REMOVE_RECURSE "${_ort_extract}")
        file(MAKE_DIRECTORY "${_ort_extract}" "${GSM_ONNXRUNTIME_ROOT}/runtime/cpu")
        file(ARCHIVE_EXTRACT INPUT "${_ort_archive}" DESTINATION "${_ort_extract}")
        set(_ort_source "${_ort_extract}/onnxruntime-linux-${_ort_arch}-1.24.4")
        file(COPY "${_ort_source}/include/" DESTINATION "${GSM_ONNXRUNTIME_ROOT}/include")
        file(COPY "${_ort_source}/lib/" DESTINATION "${GSM_ONNXRUNTIME_ROOT}/runtime/cpu"
            FILES_MATCHING PATTERN "libonnxruntime.so*")
    endif()
else()
    message(FATAL_ERROR "Unsupported platform for ONNX Runtime")
endif()

if(NOT WIN32)
    add_library(gsm_windowsml_api INTERFACE)
endif()

if(NOT EXISTS "${GSM_ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "The downloaded ONNX Runtime SDK is incomplete at ${GSM_ONNXRUNTIME_ROOT}")
endif()
if(WIN32 AND (NOT EXISTS "${GSM_ONNXRUNTIME_ROOT}/runtime/cuda/onnxruntime.dll"
              OR NOT EXISTS "${GSM_ONNXRUNTIME_ROOT}/runtime/cuda/onnxruntime_providers_tensorrt.dll"
              OR NOT EXISTS "${GSM_ONNXRUNTIME_ROOT}/runtime/directml/onnxruntime.dll"))
    message(FATAL_ERROR "The downloaded Windows ONNX Runtime is incomplete at ${GSM_ONNXRUNTIME_ROOT}")
elseif(APPLE AND NOT EXISTS "${GSM_ONNXRUNTIME_ROOT}/runtime/cpu/libonnxruntime.dylib")
    message(FATAL_ERROR "The downloaded macOS ONNX Runtime is incomplete at ${GSM_ONNXRUNTIME_ROOT}")
elseif(UNIX AND NOT APPLE AND NOT EXISTS "${GSM_ONNXRUNTIME_ROOT}/runtime/cpu/libonnxruntime.so")
    message(FATAL_ERROR "The downloaded Linux ONNX Runtime is incomplete at ${GSM_ONNXRUNTIME_ROOT}")
endif()

add_library(gsm_onnxruntime_headers INTERFACE)
target_include_directories(gsm_onnxruntime_headers INTERFACE
    "${GSM_ONNXRUNTIME_ROOT}/include")

set(GSM_ONNXRUNTIME_RUNTIME_ROOT "${GSM_ONNXRUNTIME_ROOT}/runtime" PARENT_SCOPE)
if(WIN32)
    set(GSM_WINDOWSML_RUNTIME_ROOT "${GSM_WINDOWSML_ROOT}/runtimes/win-x64/native" PARENT_SCOPE)
endif()

set(GSM_MODEL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/models" CACHE PATH
    "Path to local garment matching ONNX models")
if(NOT EXISTS "${GSM_MODEL_ROOT}/clothes_segformer_b2.onnx"
   OR NOT EXISTS "${GSM_MODEL_ROOT}/fashion_clip_vision.onnx")
    set(GSM_MODELS_AVAILABLE FALSE)
    message(STATUS
        "Garment matching models are incomplete at ${GSM_MODEL_ROOT}. "
        "Runtime inference tests and model deployment are disabled; run scripts/download-models.ps1 to enable them.")
else()
    set(GSM_MODELS_AVAILABLE TRUE)
endif()
set(GSM_MODEL_ROOT "${GSM_MODEL_ROOT}" PARENT_SCOPE)
set(GSM_MODELS_AVAILABLE "${GSM_MODELS_AVAILABLE}" PARENT_SCOPE)
