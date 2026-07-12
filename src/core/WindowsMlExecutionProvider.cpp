#include "WindowsMlExecutionProvider.h"

#ifdef Q_OS_WIN
#    include <Windows.h>

#    include <WinMLEpCatalog.h>
#endif

namespace
{
#ifdef Q_OS_WIN
    constexpr int kHresultWidth = 8;
    constexpr int kHexRadix     = 16;

    QString hresultText(HRESULT result)
    {
        return QStringLiteral("0x%1").arg(static_cast<quint32>(result), kHresultWidth, kHexRadix, QLatin1Char('0'));
    }

    class Catalog
    {
    public:
        Catalog() : m_result(WinMLEpCatalogCreate(&m_handle)) {}

        Catalog(const Catalog &)            = delete;
        Catalog &operator=(const Catalog &) = delete;
        Catalog(Catalog &&)                 = delete;
        Catalog &operator=(Catalog &&)      = delete;

        ~Catalog()
        {
            WinMLEpCatalogRelease(m_handle);
        }

        [[nodiscard]] bool valid() const
        {
            return SUCCEEDED(m_result) && m_handle;
        }

        [[nodiscard]] HRESULT result() const
        {
            return m_result;
        }

        [[nodiscard]] WinMLEpCatalogHandle handle() const
        {
            return m_handle;
        }

    private:
        WinMLEpCatalogHandle m_handle = nullptr;
        HRESULT              m_result = E_FAIL;
    };

    BOOL CALLBACK collectProvider([[maybe_unused]] WinMLEpHandle provider, const WinMLEpInfo *info, void *context)
    {
        if (!info || !info->name)
        {
            return TRUE;
        }
        auto      *items = static_cast<QVector<WindowsMlExecutionProvider::Info> *>(context);
        const auto state = info->readyState == WinMLEpReadyState_Ready      ? WindowsMlExecutionProvider::ReadyState::Ready
                           : info->readyState == WinMLEpReadyState_NotReady ? WindowsMlExecutionProvider::ReadyState::NotReady
                                                                            : WindowsMlExecutionProvider::ReadyState::NotPresent;
        items->push_back({QString::fromUtf8(info->name), QString::fromUtf8(info->version ? info->version : ""), state});
        return TRUE;
    }

    WinMLEpHandle findProvider(WinMLEpCatalogHandle catalog, const QString &requestedName)
    {
        const auto items = WindowsMlExecutionProvider::providers();
        for (const auto &item : items)
        {
            if (item.name.compare(requestedName, Qt::CaseInsensitive) == 0)
            {
                WinMLEpHandle handle = nullptr;
                if (SUCCEEDED(WinMLEpCatalogFindProvider(catalog, item.name.toUtf8().constData(), nullptr, &handle)))
                {
                    return handle;
                }
            }
        }
        return nullptr;
    }
#endif
} // namespace

QVector<WindowsMlExecutionProvider::Info> WindowsMlExecutionProvider::providers(QString *error)
{
#ifdef Q_OS_WIN
    Catalog catalog;
    if (!catalog.valid())
    {
        if (error)
        {
            *error = QStringLiteral("无法打开 Windows ML EP catalog：%1").arg(hresultText(catalog.result()));
        }
        return {};
    }
    QVector<Info> items;
    const HRESULT result = WinMLEpCatalogEnumProviders(catalog.handle(), collectProvider, &items);
    if (FAILED(result) && error)
    {
        *error = QStringLiteral("无法枚举 Windows ML EP：%1").arg(hresultText(result));
    }
    return items;
#else
    if (error)
    {
        *error = QStringLiteral("Windows ML EP 仅支持 Windows");
    }
    return {};
#endif
}

bool WindowsMlExecutionProvider::ensureReady(const QString &name, QString *error)
{
#ifdef Q_OS_WIN
    Catalog catalog;
    if (!catalog.valid())
    {
        if (error)
        {
            *error = QStringLiteral("无法打开 Windows ML EP catalog：%1").arg(hresultText(catalog.result()));
        }
        return false;
    }
    WinMLEpHandle provider = findProvider(catalog.handle(), name);
    const HRESULT result   = provider ? WinMLEpEnsureReady(provider) : HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    if (FAILED(result) && error)
    {
        *error = QStringLiteral("准备 Windows ML EP %1 失败：%2").arg(name, hresultText(result));
    }
    return SUCCEEDED(result);
#else
    Q_UNUSED(name)
    if (error)
    {
        *error = QStringLiteral("Windows ML EP 仅支持 Windows");
    }
    return false;
#endif
}

QString WindowsMlExecutionProvider::libraryPath(const QString &name, QString *error)
{
#ifdef Q_OS_WIN
    Catalog catalog;
    if (!catalog.valid())
    {
        if (error)
        {
            *error = QStringLiteral("无法打开 Windows ML EP catalog：%1").arg(hresultText(catalog.result()));
        }
        return {};
    }
    WinMLEpHandle provider = findProvider(catalog.handle(), name);
    size_t        size     = 0;
    if (!provider || FAILED(WinMLEpGetLibraryPathSize(provider, &size)) || size == 0)
    {
        if (error)
        {
            *error = QStringLiteral("Windows ML EP %1 没有可用的库路径").arg(name);
        }
        return {};
    }
    QByteArray    path(static_cast<qsizetype>(size), Qt::Uninitialized);
    const HRESULT result = WinMLEpGetLibraryPath(provider, size, path.data(), nullptr);
    if (FAILED(result))
    {
        if (error)
        {
            *error = QStringLiteral("读取 Windows ML EP %1 库路径失败：%2").arg(name, hresultText(result));
        }
        return {};
    }
    return QString::fromUtf8(path.constData());
#else
    Q_UNUSED(name)
    if (error)
    {
        *error = QStringLiteral("Windows ML EP 仅支持 Windows");
    }
    return {};
#endif
}

QString WindowsMlExecutionProvider::readyStateText(ReadyState state)
{
    return state == ReadyState::Ready      ? QStringLiteral("已就绪")
           : state == ReadyState::NotReady ? QStringLiteral("需启用")
                                           : QStringLiteral("未下载");
}
