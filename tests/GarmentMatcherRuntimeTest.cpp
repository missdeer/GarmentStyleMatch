#include <iostream>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>

#include "GarmentMatcher.h"

namespace
{

    bool check(bool condition, const QString &message)
    {
        if (condition)
        {
            return true;
        }
        std::cerr << message.toStdString() << '\n';
        return false;
    }

    bool createPlaceholderLibrary(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.putChar('\0');
    }

    // Pixel coordinates and colors are intentional properties of this synthetic garment fixture.
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-identifier-length,readability-magic-numbers)
    bool saveSyntheticImage(const QString &path, bool galleryImage, const char *format = "BMP")
    {
        QImage image(512, 512, QImage::Format_RGB888);
        image.fill(Qt::white);
        const QColor garmentColor = galleryImage ? QColor(40, 90, 210) : QColor(45, 95, 205);
        for (int y = 90; y < 300; ++y)
        {
            for (int x = 150; x < 362; ++x)
            {
                image.setPixelColor(x, y, garmentColor);
            }
        }
        for (int y = 300; y < 480; ++y)
        {
            for (int x = 175; x < 337; ++x)
            {
                image.setPixelColor(x, y, QColor(35, 40, 55));
            }
        }
        return image.save(path, format);
    }

    bool saveSolidImage(const QString &path, const QColor &color)
    {
        QImage image(512, 512, QImage::Format_RGB888);
        image.fill(color);
        return image.save(path, "BMP");
    }
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-identifier-length,readability-magic-numbers)

} // namespace

int main(int argc, char *argv[]) // NOLINT(readability-function-cognitive-complexity)
{
    QCoreApplication application(argc, argv);
    const QDir       executableDir(QCoreApplication::applicationDirPath());
    const QString    projectTmp = QDir::cleanPath(executableDir.absoluteFilePath(QStringLiteral("../../tmp")));
    if (!check(QDir().mkpath(projectTmp), QStringLiteral("无法创建项目 tmp 目录")))
    {
        return 1;
    }
    QTemporaryDir temporary(QDir(projectTmp).absoluteFilePath(QStringLiteral("runtime-test-XXXXXX")));
    if (!check(temporary.isValid(), QStringLiteral("无法创建运行时测试目录")))
    {
        return 1;
    }

    QCoreApplication::setOrganizationName(QStringLiteral("EidosTest"));
    QCoreApplication::setApplicationName(QStringLiteral("GarmentMatcherRuntimeTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QSettings().clear();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const QString requestedProvider = argc > 1 ? QString::fromLocal8Bit(argv[1]).trimmed().toLower() : QString();
#ifdef Q_OS_WIN
    if (requestedProvider.isEmpty())
    {
        const QString     probeDirectory       = QDir(temporary.path()).absoluteFilePath(QStringLiteral("provider-probe"));
        const QStringList placeholderLibraries = {
            QStringLiteral("nvcuda.dll"),
            QStringLiteral("cublas64_13.dll"),
            QStringLiteral("cublasLt64_13.dll"),
            QStringLiteral("cudnn64_9.dll"),
            QStringLiteral("cufft64_12.dll"),
            QStringLiteral("nvinfer_10.dll"),
            QStringLiteral("nvonnxparser_10.dll"),
        };
        bool placeholdersCreated = QDir().mkpath(probeDirectory);
        for (const QString &library : placeholderLibraries)
        {
            placeholdersCreated = placeholdersCreated && createPlaceholderLibrary(QDir(probeDirectory).absoluteFilePath(library));
        }
        if (!check(placeholdersCreated, QStringLiteral("无法创建推理引擎探测占位 DLL")))
        {
            return 1;
        }

        const QByteArray savedPath         = qgetenv("PATH");
        const QByteArray savedCudaPath     = qgetenv("CUDA_PATH");
        const QByteArray savedCudnnLibrary = qgetenv("CUDNN_LIBRARY");
        const QByteArray savedTensorRtRoot = qgetenv("TENSORRT_ROOT");
        qputenv("PATH", QDir::toNativeSeparators(probeDirectory).toLocal8Bit());
        qunsetenv("CUDA_PATH");
        qunsetenv("CUDNN_LIBRARY");
        qunsetenv("TENSORRT_ROOT");
        const QStringList discoverableProviders = GarmentMatcher::availableProviders();
        qputenv("PATH", savedPath);
        qputenv("CUDA_PATH", savedCudaPath);
        qputenv("CUDNN_LIBRARY", savedCudnnLibrary);
        qputenv("TENSORRT_ROOT", savedTensorRtRoot);
        if (!check(discoverableProviders.contains(QStringLiteral("CUDA")) && discoverableProviders.contains(QStringLiteral("TensorRT")),
                   QStringLiteral("推理引擎枚举必须只检查 DLL 是否可发现，不得在主窗口显示前加载大型 GPU 运行时")))
        {
            return 1;
        }
    }
#endif
    const bool verifyRestartRule = requestedProvider == QStringLiteral("restart-rule");
#ifdef Q_OS_WIN
    const QString initialProvider = verifyRestartRule             ? QStringLiteral("directml")
                                    : requestedProvider.isEmpty() ? QStringLiteral("cpu")
                                                                  : requestedProvider;
#elif defined(Q_OS_MACOS)
    const QString initialProvider = requestedProvider.isEmpty() ? QStringLiteral("cpu") : requestedProvider;
#else
    const QString initialProvider = requestedProvider;
#endif
    if (!initialProvider.isEmpty())
    {
        QSettings().setValue(QStringLiteral("matching/provider"), initialProvider);
    }
    const QString expectedProvider = GarmentMatcher::activeProvider();
    if (!verifyRestartRule && !requestedProvider.isEmpty())
    {
        const QString requestedProviderName =
            requestedProvider == QStringLiteral("tensorrt")
                ? QStringLiteral("TensorRT")
                : (requestedProvider == QStringLiteral("directml")
                       ? QStringLiteral("DirectML")
                       : (requestedProvider == QStringLiteral("coreml")
                              ? QStringLiteral("CoreML")
                              : (requestedProvider == QStringLiteral("windows ml · cpu")
                                     ? QStringLiteral("Windows ML · CPU")
                                     : (requestedProvider == QStringLiteral("windows ml · directml") ? QStringLiteral("Windows ML · DirectML")
                                                                                                     : QStringLiteral("CUDA")))));
        if (!check(expectedProvider == requestedProviderName,
                   QStringLiteral("请求的 %1 推理引擎不可用，实际选择 %2").arg(requestedProviderName, expectedProvider)))
        {
            return 1;
        }
    }

    const QString photoPath      = QDir(temporary.path()).absoluteFilePath(QStringLiteral("photo.jpg"));
    const QString blankPhotoPath = QDir(temporary.path()).absoluteFilePath(QStringLiteral("blank.bmp"));
    const QString galleryPath    = QDir(temporary.path()).absoluteFilePath(QStringLiteral("gallery.bmp"));
    if (!check(saveSyntheticImage(photoPath, false, "XPM") && saveSolidImage(blankPhotoPath, Qt::white) && saveSyntheticImage(galleryPath, true),
               QStringLiteral("无法创建合成测试图")))
    {
        return 1;
    }
    if (!check(QImageReader::imageFormat(photoPath) == QByteArrayLiteral("xpm"), QStringLiteral("实拍图必须验证 Qt 内容检测而非扩展名")))
    {
        return 1;
    }

    GarmentMatcher::Options options;
    options.segmentationModelPath                   = executableDir.absoluteFilePath(QStringLiteral("models/clothes_segformer_b2.onnx"));
    options.embeddingModelPath                      = executableDir.absoluteFilePath(QStringLiteral("models/fashion_clip_vision.onnx"));
    options.featureDatabasePath                     = QDir(temporary.path()).absoluteFilePath(QStringLiteral("features.sqlite"));
    options.categoryFilterEnabled                   = false;
    const QStringList workingDirectoryEnginesBefore = executableDir.entryList({QStringLiteral("gsm_fp16*.engine")}, QDir::Files);

    const QVector<GalleryItem>   gallery {{QStringLiteral("STYLE001"), galleryPath, QStringLiteral("test")}};
    const GarmentMatcher::Result result = GarmentMatcher::match(photoPath, gallery, options);
    std::cout << "provider=" << result.provider.toStdString() << ", success=" << (result.success ? "true" : "false")
              << ", error=" << result.error.toStdString() << '\n';

    if (!check(result.provider == expectedProvider,
               QStringLiteral("应选择 %1，实际为 %2；错误：%3").arg(expectedProvider, result.provider, result.error)))
    {
        return 1;
    }
    if (!check(result.success, QStringLiteral("ONNX 推理未完成：%1").arg(result.error)))
    {
        return 1;
    }
    if (!check(result.joinedStyleIds() == QStringLiteral("STYLE001"), QStringLiteral("应匹配 STYLE001，实际为 %1").arg(result.joinedStyleIds())))
    {
        return 1;
    }
    if (!check(result.upper.imagePath == galleryPath || result.lower.imagePath == galleryPath, QStringLiteral("匹配结果应保留获胜款号图片路径")))
    {
        return 1;
    }
    if (!check(!result.candidateDiagnostics.isEmpty() && result.candidateDiagnostics.constFirst().contains(QStringLiteral("品类约束已关闭")),
               QStringLiteral("关闭品类约束时必须保留全图库基线并输出候选数量诊断")))
    {
        return 1;
    }

    options.categoryFilterEnabled = true;
    const QVector<GalleryItem> lowerGallery {
        {QStringLiteral("STYLE001"), galleryPath, QStringLiteral("test"), QStringLiteral("lower")},
    };
    const GarmentMatcher::Result unknownPhotoResult = GarmentMatcher::match(blankPhotoPath, lowerGallery, options);
    if (!check(unknownPhotoResult.success && unknownPhotoResult.upper.styleId.isEmpty() &&
                   unknownPhotoResult.lower.styleId == QStringLiteral("STYLE001") && !unknownPhotoResult.candidateDiagnostics.isEmpty() &&
                   unknownPhotoResult.candidateDiagnostics.constFirst().contains(QStringLiteral("实拍类别 unknown")),
               QStringLiteral("分割类别 unknown 时必须使用安全候选，并按获胜图库品类写入下装而非上衣")))
    {
        return 1;
    }
    const QDateTime featureDatabaseTimestamp = QFileInfo(options.featureDatabasePath).lastModified();
    QThread::msleep(20);
    const QVector<GalleryItem> accessoryGallery {
        {QStringLiteral("STYLE001"), galleryPath, QStringLiteral("test"), QStringLiteral("accessory")},
    };
    const GarmentMatcher::Result noGarmentCandidateResult = GarmentMatcher::match(photoPath, accessoryGallery, options);
    if (!check(!noGarmentCandidateResult.success && noGarmentCandidateResult.error.contains(QStringLiteral("没有可匹配的非配件图库候选")) &&
                   !noGarmentCandidateResult.candidateDiagnostics.isEmpty() &&
                   noGarmentCandidateResult.candidateDiagnostics.constFirst().contains(QStringLiteral("回退")) &&
                   QFileInfo(options.featureDatabasePath).lastModified() == featureDatabaseTimestamp,
               QStringLiteral("仅修改分类必须刷新候选并复用既有图像向量，且零候选失败保留回退诊断")))
    {
        return 1;
    }
    options.categoryFilterEnabled = false;
    if (!check(QFileInfo(options.featureDatabasePath).size() > 0, QStringLiteral("未生成 SQLite 特征缓存")))
    {
        return 1;
    }
    if (expectedProvider == QStringLiteral("TensorRT"))
    {
        const QDir        tensorRtEngineCacheRoot(QDir(temporary.path()).absoluteFilePath(QStringLiteral("tensorrt-engines")));
        const QStringList namespaces =
            tensorRtEngineCacheRoot.entryList({QStringLiteral("ort-*_trt-*_cuda13_fp16")}, QDir::Dirs | QDir::NoDotAndDotDot);
        const QDir tensorRtEngineCache(namespaces.size() == 1 ? tensorRtEngineCacheRoot.absoluteFilePath(namespaces.front()) : QString());
        if (!check(namespaces.size() == 1, QStringLiteral("TensorRT engine 必须使用唯一的运行时版本缓存目录")) ||
            !check(!tensorRtEngineCache.entryList({QStringLiteral("gsm_fp16*.engine")}, QDir::Files).isEmpty(),
                   QStringLiteral("TensorRT FP16 engine 必须写入应用缓存目录")) ||
            !check(executableDir.entryList({QStringLiteral("gsm_fp16*.engine")}, QDir::Files) == workingDirectoryEnginesBefore,
                   QStringLiteral("TensorRT engine 不得写入当前工作目录")))
        {
            return 1;
        }
    }

    const QVector<GalleryItem>   changedGallery {{QStringLiteral("STYLE002"), galleryPath, QStringLiteral("test")}};
    const GarmentMatcher::Result changedGalleryResult = GarmentMatcher::match(photoPath, changedGallery, options);
    if (!check(changedGalleryResult.success && changedGalleryResult.joinedStyleIds() == QStringLiteral("STYLE002"),
               QStringLiteral("款号小图库变化后必须刷新常驻 Runtime 使用的图库特征")))
    {
        return 1;
    }

    if (!verifyRestartRule)
    {
        std::atomic_bool cancellationRequested = true;
        if (!check(GarmentMatcher::matchAll({photoPath, photoPath}, gallery, options, &cancellationRequested).isEmpty(),
                   QStringLiteral("批量匹配收到停止请求后不得开始下一张实拍图")))
        {
            return 1;
        }

        const QString                         missingPhotoPath = QDir(temporary.path()).absoluteFilePath(QStringLiteral("missing.jpg"));
        std::atomic_int                       completedResults = 0;
        const QVector<GarmentMatcher::Result> batchResults     = GarmentMatcher::matchAll(
            {photoPath, missingPhotoPath, photoPath}, gallery, options, nullptr, 2, [&](int, const GarmentMatcher::Result &) {
                completedResults.fetch_add(1);
            });
        if (!check(batchResults.size() == 3 && batchResults.at(0).success && !batchResults.at(1).success && batchResults.at(2).success &&
                       batchResults.at(0).joinedStyleIds() == QStringLiteral("STYLE001") &&
                       batchResults.at(2).joinedStyleIds() == QStringLiteral("STYLE001") && completedResults.load() == 3,
                   QStringLiteral("并行批量匹配必须按输入顺序返回每张实拍图的独立结果，并逐项报告完成进度")))
        {
            return 1;
        }
    }

#ifdef Q_OS_WIN
    if (verifyRestartRule)
    {
        QSettings().setValue(QStringLiteral("matching/provider"), QStringLiteral("cpu"));
        const GarmentMatcher::Result switchedResult = GarmentMatcher::match(photoPath, gallery, options);
        if (!check(switchedResult.success && switchedResult.provider == expectedProvider,
                   QStringLiteral("设置变更后同一进程必须继续使用 %1，实际为 %2；错误：%3")
                       .arg(expectedProvider, switchedResult.provider, switchedResult.error)))
        {
            return 1;
        }
    }
#endif
    return 0;
}
