#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>
#include <opencv2/imgproc.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QLibrary>
#include <QSettings>

#ifdef Q_OS_WIN
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <Windows.h>
#endif

#include "GarmentMatcher.h"
#include "SQLiteDB.h"
#include "SQLiteStatement.h"
#include "WindowsMlExecutionProvider.h"

// ONNX Runtime/OpenCV interop requires C function-pointer casts, raw image buffers, and model-defined tensor indices and dimensions.
// The surrounding size and shape checks validate every indexed access.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-const-cast,cppcoreguidelines-pro-type-reinterpret-cast,readability-identifier-length,readability-magic-numbers)
namespace
{

    constexpr int kEmbeddingSize = 512;

    using OrtGetApiBaseFunction      = const OrtApiBase *(ORT_API_CALL *)();
    using AppendDmlFunction          = OrtStatus *(ORT_API_CALL *)(OrtSessionOptions *, int);
    using GetTensorRtVersionFunction = int (*)();

    struct LoadedOrt
    {
        QLibrary          library;
        QString           preferredProvider;
        QString           version;
        AppendDmlFunction appendDml = nullptr;
        QString           windowsMlEpName;
    };

    struct Runtime
    {
        explicit Runtime(std::unique_ptr<LoadedOrt> loadedRuntime)
            : loaded(std::move(loadedRuntime)), environment(ORT_LOGGING_LEVEL_WARNING, "GarmentStyleMatch")
        {
        }

        std::unique_ptr<LoadedOrt>    loaded;
        Ort::Env                      environment;
        std::unique_ptr<Ort::Session> segmentation;
        std::unique_ptr<Ort::Session> embedding;
        QString                       provider;
    };

    [[nodiscard]] bool libraryFileIsDiscoverable(const QString &baseName)
    {
#ifdef Q_OS_WIN
        constexpr std::size_t                             kWindowsExtendedPathCapacity = 32768;
        const std::wstring                                wideName                     = baseName.toStdWString();
        std::array<wchar_t, kWindowsExtendedPathCapacity> path {};
        const DWORD length = SearchPathW(nullptr, wideName.c_str(), L".dll", static_cast<DWORD>(path.size()), path.data(), nullptr);
        return length > 0 && length < path.size();
#else
        Q_UNUSED(baseName)
        return false;
#endif
    }

    [[nodiscard]] bool hasCudaDriver()
    {
        return libraryFileIsDiscoverable(QStringLiteral("nvcuda"));
    }

    void prepareCudaDllSearchPath()
    {
        QStringList   directories;
        const QString cudaPath = QString::fromLocal8Bit(qgetenv("CUDA_PATH"));
        if (!cudaPath.isEmpty())
        {
            directories.push_back(QDir(cudaPath).absoluteFilePath(QStringLiteral("bin")));
        }

        const QString cudnnLibrary = QString::fromLocal8Bit(qgetenv("CUDNN_LIBRARY"));
        if (!cudnnLibrary.isEmpty())
        {
            QDir directory(cudnnLibrary);
            directory.cdUp();
            const QString cudaVersion = directory.dirName();
            directory.cdUp();
            directory.cdUp();
            directories.push_back(directory.absoluteFilePath(QStringLiteral("bin/%1").arg(cudaVersion)));
        }

        QByteArray path = qgetenv("PATH");
        for (const QString &directory : std::as_const(directories))
        {
            if (!QDir(directory).exists())
            {
                continue;
            }
            const QByteArray encoded = QDir::toNativeSeparators(directory).toLocal8Bit();
            if (!path.contains(encoded))
            {
                path.prepend(encoded + ';');
            }
        }
        qputenv("PATH", path);
    }

    [[nodiscard]] bool hasCudaRuntime()
    {
        if (!hasCudaDriver())
        {
            return false;
        }
        prepareCudaDllSearchPath();
        constexpr std::array<const char *, 4> dependencies {"cublas64_13", "cublasLt64_13", "cudnn64_9", "cufft64_12"};
        for (const char *dependency : dependencies)
        {
            if (!libraryFileIsDiscoverable(QString::fromLatin1(dependency)))
            {
                return false;
            }
        }
        return true;
    }

    void prepareTensorRtDllSearchPath()
    {
        const QString tensorRtRoot = QString::fromLocal8Bit(qgetenv("TENSORRT_ROOT"));
        if (tensorRtRoot.isEmpty())
        {
            return;
        }

        const QString directory = QDir(tensorRtRoot).absoluteFilePath(QStringLiteral("lib"));
        if (!QDir(directory).exists())
        {
            return;
        }

        QByteArray       path    = qgetenv("PATH");
        const QByteArray encoded = QDir::toNativeSeparators(directory).toLocal8Bit();
        if (!path.contains(encoded))
        {
            path.prepend(encoded + ';');
            qputenv("PATH", path);
        }
    }

    [[nodiscard]] QString runtimeLibraryPath(const QString &provider)
    {
#ifdef Q_OS_WIN
        if (provider == QStringLiteral("windowsml"))
        {
            return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("onnxruntime/windowsml/onnxruntime.dll"));
        }
        const QString runtimeDirectory = provider == QStringLiteral("tensorrt") ? QStringLiteral("cuda") : provider.toLower();
        return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("onnxruntime/%1/onnxruntime.dll").arg(runtimeDirectory));
#elif defined(Q_OS_MACOS)
        return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("onnxruntime/cpu/libonnxruntime.dylib"));
#else
        return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("onnxruntime/cpu/libonnxruntime.so"));
#endif
    }

    [[nodiscard]] bool hasTensorRtRuntime()
    {
#ifdef Q_OS_WIN
        if (!hasCudaRuntime() || !QFileInfo::exists(runtimeLibraryPath(QStringLiteral("tensorrt"))))
        {
            return false;
        }
        const QDir cudaRuntimeDirectory(QFileInfo(runtimeLibraryPath(QStringLiteral("tensorrt"))).absolutePath());
        if (!cudaRuntimeDirectory.exists(QStringLiteral("onnxruntime_providers_tensorrt.dll")))
        {
            return false;
        }
        prepareTensorRtDllSearchPath();
        return libraryFileIsDiscoverable(QStringLiteral("nvinfer_10")) && libraryFileIsDiscoverable(QStringLiteral("nvonnxparser_10"));
#else
        return false;
#endif
    }

    [[nodiscard]] QString tensorRtVersion()
    {
#ifdef Q_OS_WIN
        prepareTensorRtDllSearchPath();
        QLibrary library(QStringLiteral("nvinfer_10"));
        if (!library.load())
        {
            throw std::runtime_error(QStringLiteral("无法加载 TensorRT：%1").arg(library.errorString()).toStdString());
        }
        const auto getVersion = reinterpret_cast<GetTensorRtVersionFunction>(library.resolve("getInferLibVersion"));
        if (!getVersion)
        {
            throw std::runtime_error("TensorRT 缺少 getInferLibVersion");
        }
        const int version = getVersion();
        return QStringLiteral("%1.%2.%3").arg(version / 10000).arg((version % 10000) / 100).arg(version % 100);
#else
        return {};
#endif
    }

    [[nodiscard]] QString tensorRtEngineCacheNamespace(const LoadedOrt &loaded)
    {
        return QStringLiteral("ort-%1_trt-%2_cuda13_fp16").arg(loaded.version, tensorRtVersion());
    }

    [[nodiscard]] QString configuredWindowsMlProvider(const QString &configured, bool windowsMlAvailable)
    {
        if (!windowsMlAvailable)
        {
            return {};
        }
        if (configured == QStringLiteral("windows ml") || configured == QStringLiteral("windows ml - cpu") ||
            configured == QStringLiteral("windows ml · cpu"))
        {
            return QStringLiteral("windowsml-cpu");
        }
        if (configured == QStringLiteral("windows ml - directml") || configured == QStringLiteral("windows ml · directml"))
        {
            return QStringLiteral("windowsml-directml");
        }
        if (!configured.startsWith(QStringLiteral("windows ml · ")))
        {
            return {};
        }

        const QString requestedName = configured.sliced(QStringLiteral("windows ml · ").size());
        for (const auto &ep : WindowsMlExecutionProvider::providers())
        {
            if (ep.readyState != WindowsMlExecutionProvider::ReadyState::NotPresent && ep.name.compare(requestedName, Qt::CaseInsensitive) == 0)
            {
                return QStringLiteral("windowsml:%1").arg(ep.name);
            }
        }
        return {};
    }

    [[nodiscard]] QString startupProvider()
    {
        static const QString provider = [] {
#ifdef Q_OS_WIN
            const QString configured = QSettings().value(QStringLiteral("matching/provider"), QStringLiteral("auto")).toString().trimmed().toLower();
            const bool    tensorRtAvailable  = hasTensorRtRuntime();
            const bool    cudaAvailable      = hasCudaRuntime() && QFileInfo::exists(runtimeLibraryPath(QStringLiteral("cuda")));
            const bool    directMlAvailable  = QFileInfo::exists(runtimeLibraryPath(QStringLiteral("directml")));
            const bool    windowsMlAvailable = QFileInfo::exists(runtimeLibraryPath(QStringLiteral("windowsml")));
            if (configured == QStringLiteral("tensorrt") && tensorRtAvailable)
            {
                return QStringLiteral("tensorrt");
            }
            if (configured == QStringLiteral("cuda") && cudaAvailable)
            {
                return QStringLiteral("cuda");
            }
            if (configured == QStringLiteral("directml") && directMlAvailable)
            {
                return QStringLiteral("directml");
            }
            QString windowsMlProvider = configuredWindowsMlProvider(configured, windowsMlAvailable);
            if (!windowsMlProvider.isEmpty())
            {
                return windowsMlProvider;
            }
            if (configured == QStringLiteral("cpu"))
            {
                return QStringLiteral("cpu");
            }
            if (tensorRtAvailable)
            {
                return QStringLiteral("tensorrt");
            }
            if (cudaAvailable)
            {
                return QStringLiteral("cuda");
            }
            if (directMlAvailable)
            {
                return QStringLiteral("directml");
            }
#endif
            return QStringLiteral("cpu");
        }();
        return provider;
    }

    [[nodiscard]] QString runtimeBackend(const QString &provider)
    {
#ifdef Q_OS_WIN
        if (provider == QStringLiteral("cpu"))
        {
            return QFileInfo::exists(runtimeLibraryPath(QStringLiteral("directml"))) ? QStringLiteral("directml") : QStringLiteral("cuda");
        }
#endif
        return provider.startsWith(QStringLiteral("windowsml")) ? QStringLiteral("windowsml") : provider;
    }

    [[nodiscard]] std::unique_ptr<LoadedOrt> loadOrtLibrary()
    {
        const QString provider = startupProvider();
        const QString backend  = runtimeBackend(provider);
        if (backend == QStringLiteral("cuda") || backend == QStringLiteral("tensorrt"))
        {
            prepareCudaDllSearchPath();
            prepareTensorRtDllSearchPath();
        }

        auto loaded = std::make_unique<LoadedOrt>();
        loaded->library.setFileName(runtimeLibraryPath(backend));
        loaded->library.setLoadHints(QLibrary::PreventUnloadHint);
        if (!loaded->library.load())
        {
            throw std::runtime_error(QStringLiteral("无法加载 %1 ONNX Runtime：%2").arg(backend, loaded->library.errorString()).toStdString());
        }

        const auto getApiBase = reinterpret_cast<OrtGetApiBaseFunction>(loaded->library.resolve("OrtGetApiBase"));
        if (!getApiBase)
        {
            throw std::runtime_error(QStringLiteral("%1 ONNX Runtime 缺少 OrtGetApiBase").arg(backend).toStdString());
        }
        const OrtApi *api = getApiBase()->GetApi(ORT_API_VERSION);
        if (!api)
        {
            throw std::runtime_error(QStringLiteral("%1 ONNX Runtime 不支持 ORT API %2").arg(backend).arg(ORT_API_VERSION).toStdString());
        }
        Ort::InitApi(api);
        loaded->version = QString::fromLatin1(getApiBase()->GetVersionString());

        if (provider == QStringLiteral("directml") || provider == QStringLiteral("windowsml-directml"))
        {
            loaded->appendDml = reinterpret_cast<AppendDmlFunction>(loaded->library.resolve("OrtSessionOptionsAppendExecutionProvider_DML"));
            if (!loaded->appendDml)
            {
                throw std::runtime_error("DirectML ONNX Runtime 缺少 DML provider API");
            }
        }
        if (provider.startsWith(QStringLiteral("windowsml:")))
        {
            loaded->windowsMlEpName = provider.sliced(QStringLiteral("windowsml:").size());
        }
        if (provider == QStringLiteral("tensorrt"))
        {
            loaded->preferredProvider = QStringLiteral("TensorRT");
        }
        else if (provider == QStringLiteral("cuda"))
        {
            loaded->preferredProvider = QStringLiteral("CUDA");
        }
        else if (provider == QStringLiteral("directml"))
        {
            loaded->preferredProvider = QStringLiteral("DirectML");
        }
        else if (provider == QStringLiteral("windowsml-cpu"))
        {
            loaded->preferredProvider = QStringLiteral("Windows ML · CPU");
        }
        else if (provider == QStringLiteral("windowsml-directml"))
        {
            loaded->preferredProvider = QStringLiteral("Windows ML · DirectML");
        }
        else if (provider.startsWith(QStringLiteral("windowsml:")))
        {
            loaded->preferredProvider = QStringLiteral("Windows ML · %1").arg(loaded->windowsMlEpName);
        }
        else
        {
            loaded->preferredProvider = QStringLiteral("CPU");
        }
        return loaded;
    }

    struct GalleryEmbedding
    {
        QString            styleId;
        QString            imagePath;
        std::vector<float> values;
    };

    [[nodiscard]] cv::Mat loadImage(const QString &path)
    {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QImage image = reader.read().convertToFormat(QImage::Format_RGB888);
        if (image.isNull())
        {
            return {};
        }

        const cv::Mat rgb(image.height(), image.width(), CV_8UC3, const_cast<uchar *>(image.constBits()), image.bytesPerLine());
        cv::Mat       bgr;
        cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
        return bgr;
    }

    [[nodiscard]] std::vector<float> imageTensor(
        const cv::Mat &bgr, int width, int height, const std::array<float, 3> &mean, const std::array<float, 3> &standardDeviation, bool centerCrop)
    {
        cv::Mat resized;
        if (centerCrop)
        {
            const double scale = std::max(static_cast<double>(width) / bgr.cols, static_cast<double>(height) / bgr.rows);
            cv::resize(bgr,
                       resized,
                       cv::Size(static_cast<int>(std::ceil(bgr.cols * scale)), static_cast<int>(std::ceil(bgr.rows * scale))),
                       0.0,
                       0.0,
                       cv::INTER_CUBIC);
            const int left = (resized.cols - width) / 2;
            const int top  = (resized.rows - height) / 2;
            resized        = resized(cv::Rect(left, top, width, height)).clone();
        }
        else
        {
            cv::resize(bgr, resized, cv::Size(width, height), 0.0, 0.0, cv::INTER_LINEAR);
        }

        cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);
        std::vector<float> tensor(static_cast<std::size_t>(3 * width * height));
        const int          planeSize = width * height;
        for (int y = 0; y < height; ++y)
        {
            const auto *row = resized.ptr<cv::Vec3b>(y);
            for (int x = 0; x < width; ++x)
            {
                for (int channel = 0; channel < 3; ++channel)
                {
                    tensor[static_cast<std::size_t>(channel * planeSize + y * width + x)] =
                        (static_cast<float>(row[x][channel]) / 255.0F - mean[channel]) / standardDeviation[channel];
                }
            }
        }
        return tensor;
    }

    [[nodiscard]] Ort::SessionOptions sessionOptions(Ort::Env        &environment,
                                                     const QString   &provider,
                                                     const LoadedOrt &loaded,
                                                     const QString   &tensorRtEngineCachePath)
    {
        Ort::SessionOptions options;
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        if (provider == QStringLiteral("TensorRT"))
        {
            if (!QDir().mkpath(tensorRtEngineCachePath))
            {
                throw std::runtime_error(QStringLiteral("无法创建 TensorRT engine 缓存目录：%1").arg(tensorRtEngineCachePath).toStdString());
            }
            Ort::TensorRTProviderOptions tensorRtOptions;
            tensorRtOptions.Update({{"trt_engine_cache_enable", "1"},
                                    {"trt_engine_cache_path", QDir::toNativeSeparators(tensorRtEngineCachePath).toStdString()},
                                    {"trt_engine_cache_prefix", "gsm_fp16"},
                                    {"trt_fp16_enable", "1"}});
            options.AppendExecutionProvider_TensorRT_V2(*tensorRtOptions);
            OrtCUDAProviderOptions cudaOptions {};
            options.AppendExecutionProvider_CUDA(cudaOptions);
        }
        else if (provider == QStringLiteral("CUDA"))
        {
            OrtCUDAProviderOptions cudaOptions {};
            options.AppendExecutionProvider_CUDA(cudaOptions);
        }
#ifdef Q_OS_WIN
        else if (!loaded.windowsMlEpName.isEmpty())
        {
            std::vector<Ort::ConstEpDevice> devices;
            const QString                   epName = provider.sliced(QStringLiteral("Windows ML · ").size());
            for (const auto &device : environment.GetEpDevices())
            {
                if (QString::fromUtf8(device.EpName()).compare(epName, Qt::CaseInsensitive) == 0)
                {
                    devices.push_back(device);
                }
            }
            if (devices.empty())
            {
                throw std::runtime_error(QStringLiteral("Windows ML EP %1 注册后未提供可用设备").arg(epName).toStdString());
            }
            options.AppendExecutionProvider_V2(environment, devices, std::unordered_map<std::string, std::string> {});
        }
        else if (provider == QStringLiteral("DirectML") || provider == QStringLiteral("Windows ML · DirectML"))
        {
            options.DisableMemPattern();
            options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
            Ort::ThrowOnError(loaded.appendDml(options, 0));
        }
#endif
        return options;
    }

    [[nodiscard]] std::unique_ptr<Ort::Session> createSession(
        Ort::Env &environment, const QString &path, const QString &provider, const LoadedOrt &loaded, const QString &tensorRtEngineCachePath)
    {
        const Ort::SessionOptions options = sessionOptions(environment, provider, loaded, tensorRtEngineCachePath);
#ifdef Q_OS_WIN
        const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
        return std::make_unique<Ort::Session>(environment, nativePath.c_str(), options);
#else
        const QByteArray nativePath = QFile::encodeName(path);
        return std::make_unique<Ort::Session>(environment, nativePath.constData(), options);
#endif
    }

    [[nodiscard]] Runtime createRuntime(const GarmentMatcher::Options &options)
    {
        Runtime       runtime(loadOrtLibrary());
        const QString preferredProvider = runtime.loaded->preferredProvider;
#ifdef Q_OS_WIN
        if (!runtime.loaded->windowsMlEpName.isEmpty())
        {
            QString error;
            if (!WindowsMlExecutionProvider::ensureReady(runtime.loaded->windowsMlEpName, &error))
            {
                throw std::runtime_error(error.toStdString());
            }
            const QString libraryPath = WindowsMlExecutionProvider::libraryPath(runtime.loaded->windowsMlEpName, &error);
            if (libraryPath.isEmpty())
            {
                throw std::runtime_error(error.toStdString());
            }
            runtime.environment.RegisterExecutionProviderLibrary(runtime.loaded->windowsMlEpName.toUtf8().constData(), libraryPath.toStdWString());
        }
#endif
        const QString tensorRtEngineCacheRoot =
            QDir(QFileInfo(options.featureDatabasePath).absolutePath()).absoluteFilePath(QStringLiteral("tensorrt-engines"));
        const QString tensorRtEngineCachePath = preferredProvider == QStringLiteral("TensorRT")
                                                    ? QDir(tensorRtEngineCacheRoot).absoluteFilePath(tensorRtEngineCacheNamespace(*runtime.loaded))
                                                    : tensorRtEngineCacheRoot;
        QStringList   providers {preferredProvider};
        if (preferredProvider == QStringLiteral("TensorRT"))
        {
            providers.push_back(QStringLiteral("CUDA"));
        }
        if (!providers.contains(QStringLiteral("CPU")))
        {
            providers.push_back(QStringLiteral("CPU"));
        }

        QStringList errors;
        for (const QString &provider : std::as_const(providers))
        {
            try
            {
                runtime.segmentation =
                    createSession(runtime.environment, options.segmentationModelPath, provider, *runtime.loaded, tensorRtEngineCachePath);
                runtime.embedding =
                    createSession(runtime.environment, options.embeddingModelPath, provider, *runtime.loaded, tensorRtEngineCachePath);
                runtime.provider = provider;
                return runtime;
            }
            catch (const Ort::Exception &error)
            {
                runtime.segmentation.reset();
                runtime.embedding.reset();
                errors.push_back(QStringLiteral("%1: %2").arg(provider, QString::fromUtf8(error.what())));
            }
        }
        throw std::runtime_error(QStringLiteral("无法创建 ONNX Runtime Session（%1）").arg(errors.join(QStringLiteral("；"))).toStdString());
    }

    [[nodiscard]] std::vector<float> encodeImage(Ort::Session &session, const cv::Mat &image)
    {
        constexpr std::array<float, 3> mean {0.48145466F, 0.4578275F, 0.40821073F};
        constexpr std::array<float, 3> standardDeviation {0.26862954F, 0.26130258F, 0.27577711F};
        std::vector<float>             input = imageTensor(image, 224, 224, mean, standardDeviation, true);
        const std::array<int64_t, 4>   shape {1, 3, 224, 224};
        const Ort::MemoryInfo          memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value                     tensor = Ort::Value::CreateTensor<float>(memory, input.data(), input.size(), shape.data(), shape.size());
        constexpr std::array<const char *, 1> inputNames {"pixel_values"};
        constexpr std::array<const char *, 1> outputNames {"image_embeds"};
        auto outputs = session.Run(Ort::RunOptions {nullptr}, inputNames.data(), &tensor, inputNames.size(), outputNames.data(), outputNames.size());
        const auto shapeInfo   = outputs.front().GetTensorTypeAndShapeInfo();
        const auto outputShape = shapeInfo.GetShape();
        if (outputShape.size() != 2 || outputShape[0] != 1 || outputShape[1] != kEmbeddingSize)
        {
            throw std::runtime_error("FashionCLIP image_embeds 输出形状不是 [1,512]");
        }

        const auto        *values = outputs.front().GetTensorData<float>();
        std::vector<float> result(values, values + kEmbeddingSize);
        const float        norm = std::sqrt(std::inner_product(result.begin(), result.end(), result.begin(), 0.0F));
        if (norm <= 0.0F)
        {
            throw std::runtime_error("FashionCLIP 返回零向量");
        }
        for (float &value : result)
        {
            value /= norm;
        }
        return result;
    }

    [[nodiscard]] cv::Mat largestMaskComponent(const cv::Mat &mask)
    {
        cv::Mat   labels;
        cv::Mat   statistics;
        cv::Mat   centroids;
        const int count = cv::connectedComponentsWithStats(mask, labels, statistics, centroids, 8, CV_32S);
        if (count <= 1)
        {
            return {};
        }

        int largestLabel = 1;
        for (int label = 2; label < count; ++label)
        {
            if (statistics.at<int>(label, cv::CC_STAT_AREA) > statistics.at<int>(largestLabel, cv::CC_STAT_AREA))
            {
                largestLabel = label;
            }
        }
        if (statistics.at<int>(largestLabel, cv::CC_STAT_AREA) < mask.total() / 200)
        {
            return {};
        }
        cv::Mat result;
        cv::compare(labels, largestLabel, result, cv::CMP_EQ);
        return result;
    }

    [[nodiscard]] cv::Mat maskedCrop(const cv::Mat &image, const cv::Mat &smallMask)
    {
        if (smallMask.empty())
        {
            return {};
        }
        cv::Mat mask;
        cv::resize(smallMask, mask, image.size(), 0.0, 0.0, cv::INTER_NEAREST);
        mask = largestMaskComponent(mask);
        if (mask.empty())
        {
            return {};
        }

        std::vector<cv::Point> points;
        cv::findNonZero(mask, points);
        if (points.empty())
        {
            return {};
        }
        const cv::Rect bounds = cv::boundingRect(points);
        cv::Mat        crop(bounds.size(), image.type(), cv::Scalar(255, 255, 255));
        image(bounds).copyTo(crop, mask(bounds));
        return crop;
    }

    struct GarmentCrops
    {
        cv::Mat upper;
        cv::Mat lower;
    };

    [[nodiscard]] GarmentCrops segmentGarments(Ort::Session &session, const cv::Mat &image)
    {
        constexpr std::array<float, 3> mean {0.485F, 0.456F, 0.406F};
        constexpr std::array<float, 3> standardDeviation {0.229F, 0.224F, 0.225F};
        std::vector<float>             input = imageTensor(image, 512, 512, mean, standardDeviation, false);
        const std::array<int64_t, 4>   inputShape {1, 3, 512, 512};
        const Ort::MemoryInfo          memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value tensor = Ort::Value::CreateTensor<float>(memory, input.data(), input.size(), inputShape.data(), inputShape.size());
        constexpr std::array<const char *, 1> inputNames {"pixel_values"};
        constexpr std::array<const char *, 1> outputNames {"logits"};
        auto outputs = session.Run(Ort::RunOptions {nullptr}, inputNames.data(), &tensor, inputNames.size(), outputNames.data(), outputNames.size());
        const auto outputShape = outputs.front().GetTensorTypeAndShapeInfo().GetShape();
        if (outputShape.size() != 4 || outputShape[0] != 1 || outputShape[1] != 18)
        {
            throw std::runtime_error("服装分割 logits 输出形状不是 [1,18,H,W]");
        }

        const int   height    = static_cast<int>(outputShape[2]);
        const int   width     = static_cast<int>(outputShape[3]);
        const int   planeSize = height * width;
        const auto *logits    = outputs.front().GetTensorData<float>();
        cv::Mat     upperMask(height, width, CV_8U, cv::Scalar(0));
        cv::Mat     lowerMask(height, width, CV_8U, cv::Scalar(0));
        cv::Mat     dressMask(height, width, CV_8U, cv::Scalar(0));
        for (int offset = 0; offset < planeSize; ++offset)
        {
            int   bestClass = 0;
            float bestScore = logits[offset];
            for (int classIndex = 1; classIndex < 18; ++classIndex)
            {
                const float score = logits[classIndex * planeSize + offset];
                if (score > bestScore)
                {
                    bestClass = classIndex;
                    bestScore = score;
                }
            }
            const int y = offset / width;
            const int x = offset % width;
            if (bestClass == 4)
            {
                upperMask.at<uchar>(y, x) = 255;
            }
            else if (bestClass == 5 || bestClass == 6)
            {
                lowerMask.at<uchar>(y, x) = 255;
            }
            else if (bestClass == 7)
            {
                dressMask.at<uchar>(y, x) = 255;
            }
        }

        const int dressArea = cv::countNonZero(dressMask);
        if (dressArea > cv::countNonZero(upperMask) + cv::countNonZero(lowerMask))
        {
            return {maskedCrop(image, dressMask), {}};
        }
        return {maskedCrop(image, upperMask), maskedCrop(image, lowerMask)};
    }

    [[nodiscard]] QString modelKey(const QString &path)
    {
        const QFileInfo info(path);
        return QStringLiteral("%1:%2").arg(info.size()).arg(info.lastModified().toMSecsSinceEpoch());
    }

    void requireSqlite(bool success, const QString &error)
    {
        if (!success)
        {
            throw std::runtime_error(error.toStdString());
        }
    }

    [[nodiscard]] SQLiteDB openDatabase(const QString &path)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        SQLiteDB database(path);
        requireSqlite(static_cast<bool>(database), database.errorMessage());
        requireSqlite(database.execute("CREATE TABLE IF NOT EXISTS image_embeddings("
                                       "path TEXT PRIMARY KEY, style_id TEXT NOT NULL, file_size INTEGER NOT NULL, "
                                       "modified_ms INTEGER NOT NULL, model_key TEXT NOT NULL, embedding BLOB NOT NULL)"),
                      database.errorMessage());
        return database;
    }

    [[nodiscard]] std::optional<std::vector<float>> cachedEmbedding(SQLiteDB &database, const GalleryItem &item, const QString &embeddingModelKey)
    {
        const QFileInfo info(item.imagePath);
        SQLiteStatement statement = database.prepare("SELECT embedding FROM image_embeddings WHERE path=?1 AND style_id=?2 "
                                                     "AND file_size=?3 AND modified_ms=?4 AND model_key=?5");
        requireSqlite(static_cast<bool>(statement), database.errorMessage());
        requireSqlite(statement.bindText(1, item.imagePath) && statement.bindText(2, item.styleId) && statement.bindInt64(3, info.size()) &&
                          statement.bindInt64(4, info.lastModified().toMSecsSinceEpoch()) && statement.bindText(5, embeddingModelKey),
                      statement.errorMessage());
        const SQLiteStatement::StepResult stepResult = statement.step();
        if (stepResult == SQLiteStatement::StepResult::Done)
        {
            return std::nullopt;
        }
        requireSqlite(stepResult == SQLiteStatement::StepResult::Row, statement.errorMessage());
        const int bytes = statement.columnBytes(0);
        if (bytes != kEmbeddingSize * static_cast<int>(sizeof(float)))
        {
            return std::nullopt;
        }
        std::vector<float> result(kEmbeddingSize);
        std::memcpy(result.data(), statement.columnBlob(0), static_cast<std::size_t>(bytes));
        return result;
    }

    void storeEmbedding(SQLiteDB &database, const GalleryItem &item, const QString &embeddingModelKey, const std::vector<float> &embedding)
    {
        const QFileInfo info(item.imagePath);
        SQLiteStatement statement = database.prepare("INSERT OR REPLACE INTO image_embeddings"
                                                     "(path,style_id,file_size,modified_ms,model_key,embedding) VALUES(?1,?2,?3,?4,?5,?6)");
        requireSqlite(static_cast<bool>(statement), database.errorMessage());
        requireSqlite(statement.bindText(1, item.imagePath) && statement.bindText(2, item.styleId) && statement.bindInt64(3, info.size()) &&
                          statement.bindInt64(4, info.lastModified().toMSecsSinceEpoch()) && statement.bindText(5, embeddingModelKey) &&
                          statement.bindBlob(6, embedding.data(), static_cast<int>(embedding.size() * sizeof(float))),
                      statement.errorMessage());
        requireSqlite(statement.step() == SQLiteStatement::StepResult::Done, statement.errorMessage());
    }

    [[nodiscard]] QVector<GalleryEmbedding> galleryEmbeddings(Ort::Session               &session,
                                                              const QVector<GalleryItem> &items,
                                                              SQLiteDB                   &database,
                                                              const QString              &embeddingModelKey)
    {
        QVector<GalleryEmbedding> result;
        result.reserve(items.size());
        for (const GalleryItem &item : items)
        {
            std::optional<std::vector<float>> embedding = cachedEmbedding(database, item, embeddingModelKey);
            if (!embedding)
            {
                const cv::Mat image = loadImage(item.imagePath);
                if (image.empty())
                {
                    continue;
                }
                embedding = encodeImage(session, image);
                storeEmbedding(database, item, embeddingModelKey, *embedding);
            }
            result.push_back({item.styleId, item.imagePath, std::move(*embedding)});
        }
        return result;
    }

    [[nodiscard]] GarmentMatcher::Match bestMatch(const std::vector<float> &query, const QVector<GalleryEmbedding> &gallery)
    {
        GarmentMatcher::Match result;
        result.score = -1.0F;
        for (const GalleryEmbedding &candidate : gallery)
        {
            const float score = std::inner_product(query.begin(), query.end(), candidate.values.begin(), 0.0F);
            if (score > result.score)
            {
                result.styleId   = candidate.styleId;
                result.imagePath = candidate.imagePath;
                result.score     = score;
            }
        }
        return result;
    }

    void validateMatcherInputs(const QVector<GalleryItem> &galleryItems, const GarmentMatcher::Options &options)
    {
        if (galleryItems.isEmpty())
        {
            throw std::runtime_error("款号小图库为空");
        }
        if (!QFileInfo::exists(options.segmentationModelPath))
        {
            throw std::runtime_error("服装分割模型不存在");
        }
        if (!QFileInfo::exists(options.embeddingModelPath))
        {
            throw std::runtime_error("FashionCLIP 图像模型不存在");
        }
    }

    [[nodiscard]] QVector<GalleryEmbedding> prepareGallery(Runtime                       &runtime,
                                                           const QVector<GalleryItem>    &galleryItems,
                                                           const GarmentMatcher::Options &options)
    {
        SQLiteDB      database          = openDatabase(options.featureDatabasePath);
        const QString providerKey       = runtime.provider == QStringLiteral("TensorRT") ? QStringLiteral("TensorRT-FP16") : runtime.provider;
        const QString embeddingModelKey = QStringLiteral("%1:%2:%3").arg(modelKey(options.embeddingModelPath), runtime.loaded->version, providerKey);
        QVector<GalleryEmbedding> gallery = galleryEmbeddings(*runtime.embedding, galleryItems, database, embeddingModelKey);
        if (gallery.isEmpty())
        {
            throw std::runtime_error("款号小图库中没有可读取的图片");
        }
        return gallery;
    }

    struct MatcherCache
    {
        std::mutex                mutex;
        std::unique_ptr<Runtime>  runtime;
        QString                   runtimeKey;
        QVector<GalleryEmbedding> gallery;
        QString                   galleryKey;
    };

    [[nodiscard]] MatcherCache &matcherCache()
    {
        static MatcherCache cache;
        return cache;
    }

    [[nodiscard]] QString runtimeCacheKey(const GarmentMatcher::Options &options)
    {
        return QStringLiteral("%1:%2:%3:%4")
            .arg(QFileInfo(options.segmentationModelPath).absoluteFilePath(),
                 modelKey(options.segmentationModelPath),
                 QFileInfo(options.embeddingModelPath).absoluteFilePath(),
                 modelKey(options.embeddingModelPath));
    }

    [[nodiscard]] QString galleryCacheKey(const QVector<GalleryItem> &items, const Runtime &runtime)
    {
        QStringList keys;
        keys.reserve(items.size() + 2);
        keys.push_back(runtime.loaded->version);
        keys.push_back(runtime.provider);
        for (const GalleryItem &item : items)
        {
            const QFileInfo info(item.imagePath);
            keys.push_back(QStringLiteral("%1:%2:%3:%4")
                               .arg(item.styleId, info.absoluteFilePath())
                               .arg(info.size())
                               .arg(info.lastModified().toMSecsSinceEpoch()));
        }
        return keys.join(QLatin1Char('\n'));
    }

    [[nodiscard]] Runtime &cachedRuntime(MatcherCache &cache, const GarmentMatcher::Options &options)
    {
        const QString key = runtimeCacheKey(options);
        if (!cache.runtime || cache.runtimeKey != key)
        {
            cache.runtime    = std::make_unique<Runtime>(createRuntime(options));
            cache.runtimeKey = key;
            cache.gallery.clear();
            cache.galleryKey.clear();
        }
        return *cache.runtime;
    }

    [[nodiscard]] const QVector<GalleryEmbedding> &cachedGallery(MatcherCache                  &cache,
                                                                 Runtime                       &runtime,
                                                                 const QVector<GalleryItem>    &galleryItems,
                                                                 const GarmentMatcher::Options &options)
    {
        const QString key = galleryCacheKey(galleryItems, runtime);
        if (cache.gallery.isEmpty() || cache.galleryKey != key)
        {
            cache.gallery    = prepareGallery(runtime, galleryItems, options);
            cache.galleryKey = key;
        }
        return cache.gallery;
    }

    [[nodiscard]] GarmentMatcher::Result matchPhoto(const QString &photoPath, Runtime &runtime, const QVector<GalleryEmbedding> &gallery)
    {
        GarmentMatcher::Result result;
        result.provider = runtime.provider;
        try
        {
            if (!QFileInfo::exists(photoPath))
            {
                throw std::runtime_error("当前实拍图不存在");
            }
            cv::Mat photo = loadImage(photoPath);
            if (photo.empty())
            {
                throw std::runtime_error("无法读取当前实拍图");
            }

            GarmentCrops crops = segmentGarments(*runtime.segmentation, photo);
            if (crops.upper.empty() && crops.lower.empty())
            {
                throw std::runtime_error("没有分割出上衣、裤子、裙子或连衣裙");
            }
            if (!crops.upper.empty())
            {
                result.upper = bestMatch(encodeImage(*runtime.embedding, crops.upper), gallery);
            }
            if (!crops.lower.empty())
            {
                result.lower = bestMatch(encodeImage(*runtime.embedding, crops.lower), gallery);
            }
            result.success = true;
        }
        catch (const std::exception &error)
        {
            result.error = QString::fromUtf8(error.what());
        }
        return result;
    }

} // namespace

QStringList GarmentMatcher::availableProviders()
{
    QStringList providers;
#ifdef Q_OS_WIN
    if (hasTensorRtRuntime())
    {
        providers.push_back(QStringLiteral("TensorRT"));
    }
    if (hasCudaRuntime() && QFileInfo::exists(runtimeLibraryPath(QStringLiteral("cuda"))))
    {
        providers.push_back(QStringLiteral("CUDA"));
    }
    if (QFileInfo::exists(runtimeLibraryPath(QStringLiteral("directml"))))
    {
        providers.push_back(QStringLiteral("DirectML"));
    }
    if (QFileInfo::exists(runtimeLibraryPath(QStringLiteral("windowsml"))))
    {
        providers.push_back(QStringLiteral("Windows ML · DirectML"));
        providers.push_back(QStringLiteral("Windows ML · CPU"));
        for (const auto &ep : WindowsMlExecutionProvider::providers())
        {
            if (ep.readyState != WindowsMlExecutionProvider::ReadyState::NotPresent)
            {
                providers.push_back(QStringLiteral("Windows ML · %1").arg(ep.name));
            }
        }
    }
#endif
    providers.push_back(QStringLiteral("CPU"));
    return providers;
}

QString GarmentMatcher::activeProvider()
{
    const QString provider = startupProvider();
    if (provider == QStringLiteral("tensorrt"))
    {
        return QStringLiteral("TensorRT");
    }
    if (provider == QStringLiteral("cuda"))
    {
        return QStringLiteral("CUDA");
    }
    if (provider == QStringLiteral("directml"))
    {
        return QStringLiteral("DirectML");
    }
    if (provider == QStringLiteral("windowsml-cpu"))
    {
        return QStringLiteral("Windows ML · CPU");
    }
    if (provider == QStringLiteral("windowsml-directml"))
    {
        return QStringLiteral("Windows ML · DirectML");
    }
    if (provider.startsWith(QStringLiteral("windowsml:")))
    {
        return QStringLiteral("Windows ML · %1").arg(provider.sliced(QStringLiteral("windowsml:").size()));
    }
    return QStringLiteral("CPU");
}

QString GarmentMatcher::Result::joinedStyleIds() const
{
    QStringList styleIds;
    if (!upper.styleId.isEmpty())
    {
        styleIds.push_back(upper.styleId);
    }
    if (!lower.styleId.isEmpty() && lower.styleId != upper.styleId)
    {
        styleIds.push_back(lower.styleId);
    }
    return styleIds.join(QLatin1Char(','));
}

GarmentMatcher::Result GarmentMatcher::match(const QString &photoPath, const QVector<GalleryItem> &galleryItems, const Options &options)
{
    Result result;
    try
    {
        if (!QFileInfo::exists(photoPath))
        {
            throw std::runtime_error("当前实拍图不存在");
        }
        validateMatcherInputs(galleryItems, options);
        MatcherCache          &cache = matcherCache();
        const std::scoped_lock lock(cache.mutex);
        Runtime               &runtime = cachedRuntime(cache, options);
        const auto            &gallery = cachedGallery(cache, runtime, galleryItems, options);
        result                         = matchPhoto(photoPath, runtime, gallery);
    }
    catch (const std::exception &error)
    {
        result.error = QString::fromUtf8(error.what());
    }
    return result;
}

QVector<GarmentMatcher::Result> GarmentMatcher::matchAll(const QStringList          &photoPaths,
                                                         const QVector<GalleryItem> &galleryItems,
                                                         const Options              &options,
                                                         const std::atomic_bool     *cancellationRequested,
                                                         int                         parallelThreadCount)
{
    QVector<Result> results;
    if (cancellationRequested && cancellationRequested->load())
    {
        return results;
    }
    try
    {
        validateMatcherInputs(galleryItems, options);
        MatcherCache          &cache = matcherCache();
        const std::scoped_lock lock(cache.mutex);
        Runtime               &primaryRuntime = cachedRuntime(cache, options);
        const auto            &gallery        = cachedGallery(cache, primaryRuntime, galleryItems, options);
        const int              photoCount     = static_cast<int>(photoPaths.size());
        const int              workerCount    = std::clamp(parallelThreadCount, 1, std::max(1, photoCount));

        std::vector<std::unique_ptr<Runtime>> additionalRuntimes;
        additionalRuntimes.reserve(static_cast<std::size_t>(workerCount - 1));
        std::vector<Runtime *> runtimes {&primaryRuntime};
        runtimes.reserve(static_cast<std::size_t>(workerCount));
        for (int index = 1; index < workerCount; ++index)
        {
            additionalRuntimes.push_back(std::make_unique<Runtime>(createRuntime(options)));
            runtimes.push_back(additionalRuntimes.back().get());
        }

        std::vector<Result> workerResults(static_cast<std::size_t>(photoCount));
        std::atomic_int     nextIndex {0};
        std::atomic_int     claimedCount {0};
        {
            std::vector<std::jthread> workers;
            workers.reserve(static_cast<std::size_t>(workerCount));
            for (int workerIndex = 0; workerIndex < workerCount; ++workerIndex)
            {
                workers.emplace_back([&, runtime = runtimes[static_cast<std::size_t>(workerIndex)]] {
                    while (!cancellationRequested || !cancellationRequested->load())
                    {
                        const int photoIndex = nextIndex.fetch_add(1);
                        if (photoIndex >= photoCount)
                        {
                            break;
                        }
                        claimedCount.fetch_add(1);
                        workerResults[static_cast<std::size_t>(photoIndex)] = matchPhoto(photoPaths.at(photoIndex), *runtime, gallery);
                    }
                });
            }
        }
        results.reserve(claimedCount.load());
        for (int index = 0; index < claimedCount.load(); ++index)
        {
            results.push_back(std::move(workerResults[static_cast<std::size_t>(index)]));
        }
    }
    catch (const std::exception &error)
    {
        const QString message = QString::fromUtf8(error.what());
        while (results.size() < photoPaths.size())
        {
            Result result;
            result.error = message;
            results.push_back(std::move(result));
        }
    }
    return results;
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-const-cast,cppcoreguidelines-pro-type-reinterpret-cast,readability-identifier-length,readability-magic-numbers)
