#include <Windows.h>

#include <mutex>
#include <optional>
#include <WinMLEpCatalog.h>

#include "WindowsMlExecutionProvider.h"

namespace
{
    struct ProvidersCache
    {
        std::mutex                                               mutex;
        std::optional<QVector<WindowsMlExecutionProvider::Info>> items;
        QString                                                  error;
    };

    ProvidersCache &providersCache()
    {
        static ProvidersCache cache;
        return cache;
    }

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
} // namespace

QVector<WindowsMlExecutionProvider::Info> WindowsMlExecutionProvider::providers(QString *error)
{
    ProvidersCache        &cache = providersCache();
    const std::scoped_lock lock(cache.mutex);
    if (cache.items.has_value())
    {
        if (error)
        {
            *error = cache.error;
        }
        return *cache.items;
    }

    Catalog catalog;
    if (!catalog.valid())
    {
        // Transient failure — surface the error but do not cache, so the next call retries.
        if (error)
        {
            *error = QStringLiteral("无法打开 Windows ML EP catalog：%1").arg(hresultText(catalog.result()));
        }
        return {};
    }
    QVector<Info> items;
    const HRESULT result = WinMLEpCatalogEnumProviders(catalog.handle(), collectProvider, &items);
    if (FAILED(result))
    {
        // Transient failure — do not cache.
        if (error)
        {
            *error = QStringLiteral("无法枚举 Windows ML EP：%1").arg(hresultText(result));
        }
        return items;
    }
    cache.error.clear();
    cache.items = std::move(items);
    if (error)
    {
        *error = cache.error;
    }
    return *cache.items;
}

void WindowsMlExecutionProvider::invalidateCache()
{
    ProvidersCache        &cache = providersCache();
    const std::scoped_lock lock(cache.mutex);
    cache.items.reset();
    cache.error.clear();
}

bool WindowsMlExecutionProvider::ensureReady(const QString &name, QString *error)
{
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
}

QString WindowsMlExecutionProvider::libraryPath(const QString &name, QString *error)
{
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
}

QString WindowsMlExecutionProvider::readyStateText(ReadyState state)
{
    return state == ReadyState::Ready      ? QStringLiteral("已就绪")
           : state == ReadyState::NotReady ? QStringLiteral("需启用")
                                           : QStringLiteral("未下载");
}
