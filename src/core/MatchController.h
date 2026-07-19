#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

#include <QDate>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "MatchResultStore.h"

// Qt-facing declarations retain concise parameter names that mirror their property bindings.
// NOLINTBEGIN(readability-identifier-length)

class CandidateListModel;
class GalleryListModel;
class HttpDownloader;
class PhotoListModel;
class PptPageListModel;
enum class PhotoMatchStatus : std::uint8_t;
class QProcess;

class MatchController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(int currentPhotoIndex READ currentPhotoIndex WRITE setCurrentPhotoIndex NOTIFY currentPhotoIndexChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(int currentImagePage READ currentImagePage WRITE setCurrentImagePage NOTIFY currentImagePageChanged)
    Q_PROPERTY(int currentImageCount READ currentImageCount NOTIFY currentImageCountChanged)
    Q_PROPERTY(QString currentImagePath READ currentImagePath NOTIFY currentImagePathChanged)
    Q_PROPERTY(QStringList currentOutputImagePaths READ currentOutputImagePaths NOTIFY currentOutputImagePathsChanged)
    Q_PROPERTY(QString currentPhotoPath READ currentPhotoPath NOTIFY currentPhotoPathChanged)
    Q_PROPERTY(QString previousPhotoPath READ previousPhotoPath NOTIFY currentPhotoPathChanged)
    Q_PROPERTY(QString nextPhotoPath READ nextPhotoPath NOTIFY currentPhotoPathChanged)
    Q_PROPERTY(int previousPhotoUpperMatchStatus READ previousPhotoUpperMatchStatus NOTIFY adjacentPhotoMatchStatusesChanged)
    Q_PROPERTY(int previousPhotoLowerMatchStatus READ previousPhotoLowerMatchStatus NOTIFY adjacentPhotoMatchStatusesChanged)
    Q_PROPERTY(int nextPhotoUpperMatchStatus READ nextPhotoUpperMatchStatus NOTIFY adjacentPhotoMatchStatusesChanged)
    Q_PROPERTY(int nextPhotoLowerMatchStatus READ nextPhotoLowerMatchStatus NOTIFY adjacentPhotoMatchStatusesChanged)
    Q_PROPERTY(QString currentStyleId READ currentStyleId NOTIFY currentStyleIdChanged)
    Q_PROPERTY(QVariantList autoMatchedItems READ autoMatchedItems NOTIFY autoMatchedItemsChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY categoryFilterChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QString inputFilterText READ inputFilterText WRITE setInputFilterText NOTIFY inputFilterTextChanged)
    Q_PROPERTY(QString outputFilterText READ outputFilterText WRITE setOutputFilterText NOTIFY outputFilterTextChanged)
    Q_PROPERTY(QString photoDir READ photoDir WRITE setPhotoDir NOTIFY photoDirChanged)
    Q_PROPERTY(QString outputDir READ outputDir WRITE setOutputDir NOTIFY outputDirChanged)
    Q_PROPERTY(QString pptPath READ pptPath WRITE setPptPath NOTIFY pptPathChanged)
    Q_PROPERTY(bool inputTabActive READ inputTabActive WRITE activatePreview NOTIFY inputTabActiveChanged)
    Q_PROPERTY(QStringList availableUiStyles READ availableUiStyles CONSTANT)
    Q_PROPERTY(QString currentUiStyle READ currentUiStyle CONSTANT)
    Q_PROPERTY(QStringList availableInferenceEngines READ availableInferenceEngines NOTIFY availableInferenceEnginesChanged)
    Q_PROPERTY(QString currentInferenceEngine READ currentInferenceEngine NOTIFY currentInferenceEngineChanged)
    Q_PROPERTY(QVariantList windowsMlExecutionProviders READ windowsMlExecutionProviders NOTIFY windowsMlExecutionProvidersChanged)
    Q_PROPERTY(bool windowsMlEpOperationInProgress READ windowsMlEpOperationInProgress NOTIFY windowsMlEpOperationInProgressChanged)
    Q_PROPERTY(int parallelMatchThreadCount READ parallelMatchThreadCount WRITE setParallelMatchThreadCount NOTIFY parallelMatchThreadCountChanged)
    Q_PROPERTY(QString modelDirectory READ modelDirectory CONSTANT)
    Q_PROPERTY(bool modelsAvailable READ modelsAvailable NOTIFY modelsAvailableChanged)
    Q_PROPERTY(bool modelDownloadInProgress READ modelDownloadInProgress NOTIFY modelDownloadInProgressChanged)
    Q_PROPERTY(bool batchAutoMatchInProgress READ batchAutoMatchInProgress NOTIFY batchAutoMatchInProgressChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit MatchController(QObject *parent = nullptr);

    static QStringList systemUiStyles();

    void setCandidateModel(CandidateListModel *m);
    void setGalleryModel(GalleryListModel *m);
    void setPhotoModel(PhotoListModel *m);
    void setPptPageModel(PptPageListModel *m);

    [[nodiscard]] static QString title();
    [[nodiscard]] int            currentIndex() const
    {
        return m_currentIndex;
    }
    [[nodiscard]] int currentPhotoIndex() const
    {
        return m_currentPhotoIndex;
    }
    [[nodiscard]] int currentImagePage() const
    {
        return m_previewSource == PreviewPhoto ? 0 : m_currentImagePage;
    }
    [[nodiscard]] int          currentImageCount() const;
    [[nodiscard]] QString      currentImagePath() const;
    [[nodiscard]] QStringList  currentOutputImagePaths() const;
    [[nodiscard]] QString      currentPhotoPath() const;
    [[nodiscard]] QString      previousPhotoPath() const;
    [[nodiscard]] QString      nextPhotoPath() const;
    [[nodiscard]] int          previousPhotoUpperMatchStatus() const;
    [[nodiscard]] int          previousPhotoLowerMatchStatus() const;
    [[nodiscard]] int          nextPhotoUpperMatchStatus() const;
    [[nodiscard]] int          nextPhotoLowerMatchStatus() const;
    [[nodiscard]] QString      currentStyleId() const;
    [[nodiscard]] QVariantList autoMatchedItems() const
    {
        return m_autoMatchedItems;
    }
    [[nodiscard]] QString categoryFilter() const
    {
        return m_categoryFilter;
    }
    [[nodiscard]] QString searchText() const
    {
        return m_searchText;
    }
    [[nodiscard]] QString inputFilterText() const
    {
        return m_inputFilterText;
    }
    [[nodiscard]] QString outputFilterText() const
    {
        return m_outputFilterText;
    }
    [[nodiscard]] QString photoDir() const
    {
        return m_photoDir;
    }
    [[nodiscard]] QString outputDir() const
    {
        return m_outputDir;
    }
    [[nodiscard]] QString pptPath() const
    {
        return m_pptPath;
    }
    [[nodiscard]] bool inputTabActive() const
    {
        return m_previewSource == PreviewPhoto;
    }
    [[nodiscard]] QStringList availableUiStyles() const
    {
        return m_availableUiStyles;
    }
    [[nodiscard]] QString currentUiStyle() const
    {
        return m_currentUiStyle;
    }
    [[nodiscard]] QStringList availableInferenceEngines() const
    {
        return m_availableInferenceEngines;
    }
    [[nodiscard]] QString currentInferenceEngine() const
    {
        return m_currentInferenceEngine;
    }
    [[nodiscard]] QVariantList windowsMlExecutionProviders() const
    {
        return m_windowsMlExecutionProviders;
    }
    [[nodiscard]] bool windowsMlEpOperationInProgress() const
    {
        return m_windowsMlEpOperationInProgress;
    }
    [[nodiscard]] int parallelMatchThreadCount() const
    {
        return m_parallelMatchThreadCount;
    }
    [[nodiscard]] static QString modelDirectory();
    [[nodiscard]] static QString applicationModelDirectory();
    [[nodiscard]] static QString findAvailableModelDirectory(const QString &applicationModelsDir, const QString &localModelsDir);
    [[nodiscard]] static bool    modelFilesExistInDirectories(const QString &applicationModelsDir, const QString &localModelsDir);
    [[nodiscard]] static QString availableModelDirectory();
    [[nodiscard]] static bool    modelsAvailable();
    [[nodiscard]] bool           modelDownloadInProgress() const
    {
        return m_modelDownloadInProgress;
    }
    [[nodiscard]] bool batchAutoMatchInProgress() const
    {
        return m_batchAutoMatchInProgress;
    }
    [[nodiscard]] bool busy() const
    {
        return m_busy;
    }

    void setCurrentIndex(int idx);
    void setCurrentPhotoIndex(int idx);
    void setCurrentImagePage(int page);
    void setCategoryFilter(const QString &v);
    void setSearchText(const QString &v);
    void setInputFilterText(const QString &v);
    void setOutputFilterText(const QString &v);
    void setPhotoDir(const QString &v);
    void setOutputDir(const QString &v);
    void setPptPath(const QString &v);
    void setParallelMatchThreadCount(int count);

public slots:
    bool                      setCurrentUiStyle(const QString &style);
    bool                      setCurrentInferenceEngine(const QString &engine);
    void                      refreshWindowsMlExecutionProviders();
    void                      installWindowsMlExecutionProvider(const QString &name);
    bool                      useWindowsMlExecutionProvider(const QString &name);
    [[nodiscard]] static bool modelFilesExist();
    void                      downloadModels();
    void                      cancelModelDownload();
    static void               openModelDirectory();

    void restorePersistentState();
    void completeDeferredStartup();
    void notifyMainWindowShown();
    void activatePreview(bool inputTabActive);

    void previousImage(bool inputTabActive = false);
    void nextImage(bool inputTabActive = false);
    void previousUnmatchedPhoto();
    void nextUnmatchedPhoto();
    void previousUnconfirmedPhoto();
    void nextUnconfirmedPhoto();
    void openCurrentImageExternally() const;

    void previousCandidate();
    void nextCandidate();

    void               autoMatchStyleIds();
    void               autoMatchAllStyleIds();
    void               cancelAutoMatchAllStyleIds();
    [[nodiscard]] bool galleryMatchWouldOverwriteConfirmedStyleId(const QString &part) const;
    bool               matchGalleryItemToCurrentPhoto(int galleryRow, const QString &part, bool overwriteConfirmed, bool confirmed);
    [[nodiscard]] bool copyWouldOverwriteConfirmedStyleIds(int offset, const QString &part, bool targetAdjacent) const;
    bool               copyAdjacentStyleIds(int offset, const QString &part, const QString &confirmedPolicy);
    bool               copyStyleIdsToAdjacent(int offset, const QString &part, const QString &confirmedPolicy);
    void               confirmAutoMatch(const QString &part);
    void               rejectAutoMatch(const QString &part);
    void               generateFineTuneModel();

    void scanPhotoDir();
    void scanOutputDir();
    void reloadPpt();
    void togglePptPageSelected(int row);
    void extractFromSelectedPages();

signals:
    void subtitleChanged();
    void currentIndexChanged();
    void currentPhotoIndexChanged();
    void currentImagePageChanged();
    void currentImageCountChanged();
    void currentImagePathChanged();
    void currentOutputImagePathsChanged();
    void currentPhotoPathChanged();
    void adjacentPhotoMatchStatusesChanged();
    void currentStyleIdChanged();
    void autoMatchedItemsChanged();
    void categoryFilterChanged();
    void searchTextChanged();
    void inputFilterTextChanged();
    void outputFilterTextChanged();
    void photoDirChanged();
    void outputDirChanged();
    void pptPathChanged();
    void inputTabActiveChanged();
    void modelsAvailableChanged();
    void modelDownloadInProgressChanged();
    void batchAutoMatchInProgressChanged();
    void parallelMatchThreadCountChanged();
    void availableInferenceEnginesChanged();
    void currentInferenceEngineChanged();
    void windowsMlExecutionProvidersChanged();
    void windowsMlEpOperationInProgressChanged();
    void deferredStartupCompleted();
    void modelDownloadRequired();
    void busyChanged();
    void mainWindowShown();

    void logMessage(const QString &msg);

private:
    enum class PhotoNavigationFilter : std::uint8_t
    {
        Unmatched,
        Unconfirmed,
    };

    void                  emitCurrentChanged();
    void                  handleWatchedDirectoryChanged(const QString &path);
    void                  updateWatchedDirectories();
    void                  scanPhotoDir(bool skipIfEntriesUnchanged);
    void                  scanOutputDir(bool skipIfEntriesUnchanged);
    void                  clearAutoMatchResult();
    void                  restoreAutoMatchResult();
    void                  rebuildAutoMatchedItems();
    void                  refreshPhotoMatchStatuses();
    void                  updatePhotoMatchStatuses(const QString &imagePath, const StoredMatchResult &result);
    [[nodiscard]] int     adjacentPhotoMatchStatus(int offset, bool upper) const;
    void                  navigatePhoto(int direction, PhotoNavigationFilter filter);
    bool                  persistAutoMatchResult(QString *error = nullptr) const;
    [[nodiscard]] QString matchDatabasePath() const;
    [[nodiscard]] QString galleryImagePath(const StoredGarmentMatch &match) const;

    enum PreviewSource : std::uint8_t
    {
        PreviewPhoto,
        PreviewOutput
    };

    CandidateListModel *m_candidateModel = nullptr;
    GalleryListModel   *m_galleryModel   = nullptr;
    PhotoListModel     *m_photoModel     = nullptr;
    PptPageListModel   *m_pptPageModel   = nullptr;

    int           m_currentIndex      = -1;
    int           m_currentPhotoIndex = -1;
    int           m_currentImagePage  = 0; // 0-based
    PreviewSource m_previewSource     = PreviewPhoto;

    QString                           m_categoryFilter = QStringLiteral("\xE5\x85\xA8\xE9\x83\xA8"); // "全部"
    QString                           m_searchText;
    QString                           m_inputFilterText;
    QString                           m_outputFilterText;
    QString                           m_photoDir;
    QString                           m_outputDir;
    QFileSystemWatcher                m_directoryWatcher;
    QStringList                       m_photoEntrySnapshot;
    QStringList                       m_outputEntrySnapshot;
    QString                           m_pptPath;
    QStringList                       m_availableUiStyles;
    QString                           m_currentUiStyle;
    QStringList                       m_availableInferenceEngines;
    QString                           m_currentInferenceEngine;
    QVariantList                      m_windowsMlExecutionProviders;
    bool                              m_windowsMlEpOperationInProgress = false;
    int                               m_parallelMatchThreadCount       = 1;
    QVariantList                      m_autoMatchedItems;
    StoredMatchResult                 m_autoMatchResult;
    QString                           m_autoMatchImagePath;
    bool                              m_busy                               = false;
    bool                              m_restoringPptState                  = false;
    bool                              m_modelDownloadInProgress            = false;
    bool                              m_batchAutoMatchInProgress           = false;
    bool                              m_modelDownloadCancellationRequested = false;
    int                               m_modelDownloadIndex                 = 0;
    HttpDownloader                   *m_httpDownloader                     = nullptr;
    QProcess                         *m_modelDownloadProcess               = nullptr;
    QString                           m_pythonExecutable;
    QString                           m_pythonPackagesDir;
    std::shared_ptr<std::atomic_bool> m_batchAutoMatchCancellation;
    bool                              m_deferredStartupCompleted = false;

    void                         setBusy(bool on);
    void                         setBatchAutoMatchInProgress(bool inProgress);
    void                         startNextModelDownload();
    void                         startPythonDependencyInstall();
    void                         startPythonModelExtraction();
    void                         connectModelProcessOutput(QProcess *process);
    void                         finishModelDownload(const QString &message);
    [[nodiscard]] static QString pptCacheDir(const QString &pptFilePath);
    [[nodiscard]] static QString pptPagesSettingsKey(const QString &pptFilePath);
    void                         persistSelectedPptPages();
    void                         restoreSelectedPptPages();
    bool                         loadPptPreviewsFromCache();
    bool                         loadPptStylesFromCache();
};
// NOLINTEND(readability-identifier-length)
