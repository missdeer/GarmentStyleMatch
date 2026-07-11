#pragma once

#include <functional>

#include <QString>
#include <QStringList>
#include <QVector>

struct ExtractedStyle
{
    QString          styleId;
    int              sourcePage = 0;
    QVector<QString> imagePaths;
};

class PptStyleExtractor
{
public:
    struct Options
    {
        QString                                        pptxPath;
        QVector<int>                                   pages;
        QString                                        outputDir;
        QString                                        openXmlDir;
        std::function<void(int, int, const QString &)> progress;
    };

    struct Result
    {
        QVector<ExtractedStyle> styles;
        QStringList             warnings;
    };

    static Result extract(const Options &options);
};
