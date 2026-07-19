#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSettings>

#include "core/CandidateListModel.h"
#include "core/GalleryListModel.h"
#include "core/ImageMetadata.h"
#include "core/MatchController.h"
#include "core/PhotoListModel.h"
#include "platform/SplashScreen.h"
#ifdef Q_OS_WIN
#    include "core/PptPageListModel.h"
#endif

int main(int argc, char *argv[])
{
    SplashScreen::show();

    QGuiApplication::setOrganizationName(QStringLiteral("Eidos"));
    QGuiApplication::setApplicationName(QStringLiteral("GarmentStyleMatch"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QGuiApplication app(argc, argv);

    const QStringList availableUiStyles = MatchController::systemUiStyles();
    const QString     savedUiStyle      = QSettings().value(QStringLiteral("ui/style"), QStringLiteral("FluentWinUI3")).toString();
    QString           selectedUiStyle   = QStringLiteral("FluentWinUI3");
    for (const QString &availableUiStyle : availableUiStyles)
    {
        if (availableUiStyle.compare(savedUiStyle, Qt::CaseInsensitive) == 0)
        {
            selectedUiStyle = availableUiStyle;
            break;
        }
    }
    QQuickStyle::setStyle(selectedUiStyle);

    CandidateListModel candidateModel;
    GalleryListModel   galleryModel;
    PhotoListModel     photoModel;
#ifdef Q_OS_WIN
    PptPageListModel pptPageModel;
#endif
    ImageMetadata   imageMetadata;
    MatchController controller;
    controller.setCandidateModel(&candidateModel);
    controller.setGalleryModel(&galleryModel);
    controller.setPhotoModel(&photoModel);
#ifdef Q_OS_WIN
    controller.setPptPageModel(&pptPageModel);
#endif

    const auto refreshImageMetadata = [&controller, &imageMetadata] { imageMetadata.setImagePath(controller.currentPhotoPath()); };
    QObject::connect(&controller, &MatchController::currentPhotoPathChanged, &imageMetadata, refreshImageMetadata);

    QObject::connect(&controller, &MatchController::mainWindowShown, &app, [] { SplashScreen::hide(); });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("candidateModel"), &candidateModel);
    engine.rootContext()->setContextProperty(QStringLiteral("galleryModel"), &galleryModel);
    engine.rootContext()->setContextProperty(QStringLiteral("photoModel"), &photoModel);
#ifdef Q_OS_WIN
    engine.rootContext()->setContextProperty(QStringLiteral("pptPageModel"), &pptPageModel);
#else
    engine.rootContext()->setContextProperty(QStringLiteral("pptPageModel"), QVariant());
#endif
    engine.rootContext()->setContextProperty(QStringLiteral("imageMetadata"), &imageMetadata);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("GarmentStyleMatch", "Main");

    return QGuiApplication::exec();
}
