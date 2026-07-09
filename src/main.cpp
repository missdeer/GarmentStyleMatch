#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "core/MatchController.h"
#include "core/CandidateListModel.h"
#include "core/GalleryListModel.h"
#include "core/PhotoListModel.h"

int main(int argc, char *argv[])
{
    QGuiApplication::setOrganizationName(QStringLiteral("GarmentStyleMatch"));
    QGuiApplication::setApplicationName(QStringLiteral("GarmentStyleMatch"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QGuiApplication app(argc, argv);

    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    CandidateListModel candidateModel;
    GalleryListModel   galleryModel;
    PhotoListModel     photoModel;
    MatchController    controller;
    controller.setCandidateModel(&candidateModel);
    controller.setGalleryModel(&galleryModel);
    controller.setPhotoModel(&photoModel);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("controller"),     &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("candidateModel"), &candidateModel);
    engine.rootContext()->setContextProperty(QStringLiteral("galleryModel"),   &galleryModel);
    engine.rootContext()->setContextProperty(QStringLiteral("photoModel"),     &photoModel);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);

    engine.loadFromModule("GarmentStyleMatch", "Main");

    return app.exec();
}
