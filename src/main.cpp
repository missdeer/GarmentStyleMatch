#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSettings>

#include "core/MatchController.h"
#include "core/CandidateListModel.h"
#include "core/GalleryListModel.h"
#include "core/ImageMetadata.h"
#include "core/PhotoListModel.h"
#include "core/PptPageListModel.h"

int main(int argc, char *argv[])
{
    QGuiApplication::setOrganizationName(QStringLiteral("Eidos"));
    QGuiApplication::setApplicationName(QStringLiteral("GarmentStyleMatch"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QGuiApplication app(argc, argv);

    const QStringList availableUiStyles = MatchController::systemUiStyles();
    const QString savedUiStyle = QSettings().value(
        QStringLiteral("ui/style"), QStringLiteral("Fusion")).toString();
    QString selectedUiStyle = QStringLiteral("Fusion");
    for (const QString &availableUiStyle : availableUiStyles) {
        if (availableUiStyle.compare(savedUiStyle, Qt::CaseInsensitive) == 0) {
            selectedUiStyle = availableUiStyle;
            break;
        }
    }
    QQuickStyle::setStyle(selectedUiStyle);

    CandidateListModel candidateModel;
    GalleryListModel   galleryModel;
    PhotoListModel     photoModel;
    PptPageListModel   pptPageModel;
    ImageMetadata      imageMetadata;
    MatchController    controller;
    controller.setCandidateModel(&candidateModel);
    controller.setGalleryModel(&galleryModel);
    controller.setPhotoModel(&photoModel);
    controller.setPptPageModel(&pptPageModel);

    const auto refreshImageMetadata = [&controller, &imageMetadata] {
        imageMetadata.setImagePath(controller.currentPhotoPath());
    };
    QObject::connect(&controller, &MatchController::currentPhotoPathChanged,
                     &imageMetadata, refreshImageMetadata);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("controller"),     &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("candidateModel"), &candidateModel);
    engine.rootContext()->setContextProperty(QStringLiteral("galleryModel"),   &galleryModel);
    engine.rootContext()->setContextProperty(QStringLiteral("photoModel"),     &photoModel);
    engine.rootContext()->setContextProperty(QStringLiteral("pptPageModel"),   &pptPageModel);
    engine.rootContext()->setContextProperty(QStringLiteral("imageMetadata"),  &imageMetadata);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);

    engine.loadFromModule("GarmentStyleMatch", "Main");

    return app.exec();
}
