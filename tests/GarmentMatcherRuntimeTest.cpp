#include <iostream>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QLibrary>
#include <QSettings>
#include <QTemporaryDir>

#include "GarmentMatcher.h"

namespace
{

    bool check(bool condition, const QString &message)
    {
        if (condition)
            return true;
        std::cerr << message.toStdString() << '\n';
        return false;
    }

    bool saveSyntheticImage(const QString &path, bool galleryImage)
    {
        QImage image(512, 512, QImage::Format_RGB888);
        image.fill(Qt::white);
        const QColor garmentColor = galleryImage ? QColor(40, 90, 210) : QColor(45, 95, 205);
        for (int y = 90; y < 300; ++y)
        {
            for (int x = 150; x < 362; ++x)
                image.setPixelColor(x, y, garmentColor);
        }
        for (int y = 300; y < 480; ++y)
        {
            for (int x = 175; x < 337; ++x)
                image.setPixelColor(x, y, QColor(35, 40, 55));
        }
        return image.save(path, "BMP");
    }

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QDir       executableDir(QCoreApplication::applicationDirPath());
    const QString    projectTmp = QDir::cleanPath(executableDir.absoluteFilePath(QStringLiteral("../../tmp")));
    if (!check(QDir().mkpath(projectTmp), QStringLiteral("无法创建项目 tmp 目录")))
        return 1;
    QTemporaryDir temporary(QDir(projectTmp).absoluteFilePath(QStringLiteral("runtime-test-XXXXXX")));
    if (!check(temporary.isValid(), QStringLiteral("无法创建运行时测试目录")))
        return 1;

    QCoreApplication::setOrganizationName(QStringLiteral("EidosTest"));
    QCoreApplication::setApplicationName(QStringLiteral("GarmentMatcherRuntimeTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QSettings().clear();
    const QString requestedProvider = argc > 1 ? QString::fromLocal8Bit(argv[1]).trimmed().toLower() : QString();
    const bool    switchProviders   = requestedProvider == QStringLiteral("switch");
#ifdef Q_OS_WIN
    QLibrary      cudaDriver(QStringLiteral("nvcuda"));
    const bool    hasCuda         = cudaDriver.load();
    const QString initialProvider = switchProviders ? (hasCuda ? QStringLiteral("cuda") : QStringLiteral("directml")) : requestedProvider;
#else
    const QString initialProvider = requestedProvider;
#endif
    if (!initialProvider.isEmpty())
        QSettings().setValue(QStringLiteral("matching/provider"), initialProvider);

    const QString photoPath   = QDir(temporary.path()).absoluteFilePath(QStringLiteral("photo.bmp"));
    const QString galleryPath = QDir(temporary.path()).absoluteFilePath(QStringLiteral("gallery.bmp"));
    if (!check(saveSyntheticImage(photoPath, false) && saveSyntheticImage(galleryPath, true), QStringLiteral("无法创建合成测试图")))
        return 1;

    GarmentMatcher::Options options;
    options.segmentationModelPath = executableDir.absoluteFilePath(QStringLiteral("models/clothes_segformer_b2.onnx"));
    options.embeddingModelPath    = executableDir.absoluteFilePath(QStringLiteral("models/fashion_clip_vision.onnx"));
    options.featureDatabasePath   = QDir(temporary.path()).absoluteFilePath(QStringLiteral("features.sqlite"));

    const QVector<GalleryItem>   gallery {{QStringLiteral("STYLE001"), galleryPath, QStringLiteral("test")}};
    const GarmentMatcher::Result result = GarmentMatcher::match(photoPath, gallery, options);
    std::cout << "provider=" << result.provider.toStdString() << ", success=" << (result.success ? "true" : "false")
              << ", error=" << result.error.toStdString() << '\n';

#ifdef Q_OS_WIN
    const QString expectedProvider =
        initialProvider == QStringLiteral("cpu")
            ? QStringLiteral("CPU")
            : (initialProvider == QStringLiteral("directml") ? QStringLiteral("DirectML")
                                                             : (hasCuda ? QStringLiteral("CUDA") : QStringLiteral("DirectML")));
#else
    const QString expectedProvider = QStringLiteral("CPU");
#endif
    if (!check(result.provider == expectedProvider,
               QStringLiteral("应选择 %1，实际为 %2；错误：%3").arg(expectedProvider, result.provider, result.error)))
        return 1;
    if (!check(result.success, QStringLiteral("ONNX 推理未完成：%1").arg(result.error)))
        return 1;
    if (!check(result.joinedStyleIds() == QStringLiteral("STYLE001"), QStringLiteral("应匹配 STYLE001，实际为 %1").arg(result.joinedStyleIds())))
        return 1;
    if (!check(QFileInfo(options.featureDatabasePath).size() > 0, QStringLiteral("未生成 SQLite 特征缓存")))
        return 1;

#ifdef Q_OS_WIN
    if (switchProviders)
    {
        const QString nextSetting  = expectedProvider == QStringLiteral("CUDA") ? QStringLiteral("directml") : QStringLiteral("cpu");
        const QString nextExpected = nextSetting == QStringLiteral("directml") ? QStringLiteral("DirectML") : QStringLiteral("CPU");
        QSettings().setValue(QStringLiteral("matching/provider"), nextSetting);
        const GarmentMatcher::Result switchedResult = GarmentMatcher::match(photoPath, gallery, options);
        if (!check(switchedResult.success && switchedResult.provider == nextExpected,
                   QStringLiteral("同进程切换后应使用 %1，实际为 %2；错误：%3").arg(nextExpected, switchedResult.provider, switchedResult.error)))
            return 1;
    }
#endif
    return 0;
}
