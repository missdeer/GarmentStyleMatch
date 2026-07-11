#pragma once

#include <cstdint>

#include <QDate>
#include <QObject>
#include <QString>
#include <QStringList>

// Qt-facing declarations retain concise parameter names that mirror their property bindings.
// NOLINTBEGIN(readability-identifier-length)

class CandidateListModel;
class GalleryListModel;
class PhotoListModel;
class PptPageListModel;

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
    Q_PROPERTY(QString currentStyleId READ currentStyleId NOTIFY currentStyleIdChanged)
    Q_PROPERTY(QString autoMatchedStyleIds READ autoMatchedStyleIds NOTIFY autoMatchedStyleIdsChanged)
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
    Q_PROPERTY(QStringList availableInferenceEngines READ availableInferenceEngines CONSTANT)
    Q_PROPERTY(QString currentInferenceEngine READ currentInferenceEngine CONSTANT)
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
    [[nodiscard]] int         currentImageCount() const;
    [[nodiscard]] QString     currentImagePath() const;
    [[nodiscard]] QStringList currentOutputImagePaths() const;
    [[nodiscard]] QString     currentPhotoPath() const;
    [[nodiscard]] QString     currentStyleId() const;
    [[nodiscard]] QString     autoMatchedStyleIds() const
    {
        return m_autoMatchedStyleIds;
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

public slots:
    bool setCurrentUiStyle(const QString &style);
    bool setCurrentInferenceEngine(const QString &engine);

    void loadDemoData();
    void restorePersistentState();
    void activatePreview(bool inputTabActive);

    void previousImage(bool inputTabActive = false);
    void nextImage(bool inputTabActive = false);
    void openCurrentImageExternally() const;

    void previousCandidate();
    void nextCandidate();

    void confirmSelectedThumb(int galleryRow);
    void confirmStyleId(const QString &styleId);
    void autoMatchStyleIds();
    void generateFineTuneModel();

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
    void currentStyleIdChanged();
    void autoMatchedStyleIdsChanged();
    void categoryFilterChanged();
    void searchTextChanged();
    void inputFilterTextChanged();
    void outputFilterTextChanged();
    void photoDirChanged();
    void outputDirChanged();
    void pptPathChanged();
    void inputTabActiveChanged();
    void busyChanged();

    void logMessage(const QString &msg);

private:
    void emitCurrentChanged();

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

    QString     m_categoryFilter = QStringLiteral("\xE5\x85\xA8\xE9\x83\xA8"); // "全部"
    QString     m_searchText;
    QString     m_inputFilterText;
    QString     m_outputFilterText;
    QString     m_photoDir;
    QString     m_outputDir;
    QString     m_pptPath;
    QStringList m_availableUiStyles;
    QString     m_currentUiStyle;
    QStringList m_availableInferenceEngines;
    QString     m_currentInferenceEngine;
    QString     m_autoMatchedStyleIds;
    bool        m_busy              = false;
    bool        m_restoringPptState = false;

    void                         setBusy(bool on);
    [[nodiscard]] static QString pptCacheDir(const QString &pptFilePath);
    [[nodiscard]] static QString pptPagesSettingsKey(const QString &pptFilePath);
    void                         persistSelectedPptPages();
    void                         restoreSelectedPptPages();
    bool                         loadPptPreviewsFromCache();
    bool                         loadPptStylesFromCache();
};
// NOLINTEND(readability-identifier-length)
