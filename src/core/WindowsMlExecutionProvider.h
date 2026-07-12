#pragma once

#include <QString>
#include <QVector>

class WindowsMlExecutionProvider
{
public:
    enum class ReadyState
    {
        Ready,
        NotReady,
        NotPresent,
    };

    struct Info
    {
        QString    name;
        QString    version;
        ReadyState readyState = ReadyState::NotPresent;
    };

    [[nodiscard]] static QVector<Info> providers(QString *error = nullptr);
    [[nodiscard]] static bool          ensureReady(const QString &name, QString *error = nullptr);
    [[nodiscard]] static QString       libraryPath(const QString &name, QString *error = nullptr);
    [[nodiscard]] static QString       readyStateText(ReadyState state);
};
