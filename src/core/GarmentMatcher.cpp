#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <sqlite3.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QSettings>

#include "GarmentMatcher.h"

namespace
{

    constexpr int kEmbeddingSize = 512;

    struct SqliteDeleter
    {
        void operator()(sqlite3 *database) const
        {
            if (database)
                sqlite3_close(database);
        }
    };

    struct StatementDeleter
    {
        void operator()(sqlite3_stmt *statement) const
        {
            if (statement)
                sqlite3_finalize(statement);
        }
    };

    using Database  = std::unique_ptr<sqlite3, SqliteDeleter>;
    using Statement = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

    using OrtGetApiBaseFunction = const OrtApiBase *(ORT_API_CALL *)();
    using AppendDmlFunction     = OrtStatus *(ORT_API_CALL *)(OrtSessionOptions *, int);

    struct LoadedOrt
    {
        QLibrary          library;
        QString           preferredProvider;
        AppendDmlFunction appendDml = nullptr;
    };

    struct Runtime
    {
        explicit Runtime(std::unique_ptr<LoadedOrt> loadedRuntime)
            : loaded(std::move(loadedRuntime)), environment(ORT_LOGGING_LEVEL_WARNING, "GarmentStyleMatch")
        {
        }

        std::unique_ptr<LoadedOrt>    loaded;
        Ort::Env                      environment;
        std::unique_ptr<Ort::Session> segmentation;
        std::unique_ptr<Ort::Session> embedding;
        QString                       provider;
    };

    [[nodiscard]] bool hasCudaDriver()
    {
        QLibrary cudaDriver(QStringLiteral("nvcuda"));
        return cudaDriver.load();
    }

    void prepareCudaDllSearchPath()
    {
        QStringList   directories;
        const QString cudaPath = QString::fromLocal8Bit(qgetenv("CUDA_PATH"));
        if (!cudaPath.isEmpty())
            directories.push_back(QDir(cudaPath).absoluteFilePath(QStringLiteral("bin")));

        const QString cudnnLibrary = QString::fromLocal8Bit(qgetenv("CUDNN_LIBRARY"));
        if (!cudnnLibrary.isEmpty())
        {
            QDir directory(cudnnLibrary);
            directory.cdUp();
            const QString cudaVersion = directory.dirName();
            directory.cdUp();
            directory.cdUp();
            directories.push_back(directory.absoluteFilePath(QStringLiteral("bin/%1").arg(cudaVersion)));
        }

        QByteArray path = qgetenv("PATH");
        for (const QString &directory : std::as_const(directories))
        {
            if (!QDir(directory).exists())
                continue;
            const QByteArray encoded = QDir::toNativeSeparators(directory).toLocal8Bit();
            if (!path.contains(encoded))
                path.prepend(encoded + ';');
        }
        qputenv("PATH", path);
    }

    [[nodiscard]] QString runtimeLibraryPath(const QString &provider)
    {
#ifdef Q_OS_WIN
        return QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(QStringLiteral("onnxruntime/%1/onnxruntime.dll").arg(provider.toLower()));
#elif defined(Q_OS_MACOS)
        return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("onnxruntime/cpu/libonnxruntime.dylib"));
#else
        return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("onnxruntime/cpu/libonnxruntime.so"));
#endif
    }

    [[nodiscard]] std::unique_ptr<LoadedOrt> loadOrtLibrary()
    {
        const QString configured = QSettings().value(QStringLiteral("matching/provider"), QStringLiteral("auto")).toString().trimmed().toLower();
        QStringList   candidates;
#ifdef Q_OS_WIN
        if (configured == QStringLiteral("cuda"))
            candidates = {QStringLiteral("cuda")};
        else if (configured == QStringLiteral("directml"))
            candidates = {QStringLiteral("directml")};
        else if (configured == QStringLiteral("cpu"))
            candidates = {hasCudaDriver() ? QStringLiteral("cuda") : QStringLiteral("directml")};
        else
            candidates = hasCudaDriver() ? QStringList {QStringLiteral("cuda"), QStringLiteral("directml")}
                                         : QStringList {QStringLiteral("directml"), QStringLiteral("cuda")};
#else
        candidates = {QStringLiteral("cpu")};
#endif

        QStringList errors;
        for (const QString &candidate : std::as_const(candidates))
        {
            if (candidate == QStringLiteral("cuda"))
                prepareCudaDllSearchPath();
            const QString path   = runtimeLibraryPath(candidate);
            auto          loaded = std::make_unique<LoadedOrt>();
            loaded->library.setFileName(path);
            loaded->library.setLoadHints(QLibrary::PreventUnloadHint);
            if (!loaded->library.load())
            {
                errors.push_back(QStringLiteral("%1: %2").arg(candidate, loaded->library.errorString()));
                continue;
            }

            const auto getApiBase = reinterpret_cast<OrtGetApiBaseFunction>(loaded->library.resolve("OrtGetApiBase"));
            if (!getApiBase)
            {
                errors.push_back(QStringLiteral("%1: 缺少 OrtGetApiBase").arg(candidate));
                continue;
            }
            const OrtApi *api = getApiBase()->GetApi(ORT_API_VERSION);
            if (!api)
            {
                errors.push_back(QStringLiteral("%1: 不支持 ORT API %2").arg(candidate).arg(ORT_API_VERSION));
                continue;
            }
            Ort::InitApi(api);
            if (candidate == QStringLiteral("directml"))
            {
                loaded->appendDml = reinterpret_cast<AppendDmlFunction>(loaded->library.resolve("OrtSessionOptionsAppendExecutionProvider_DML"));
                if (!loaded->appendDml)
                {
                    errors.push_back(QStringLiteral("directml: 缺少 DML provider API"));
                    continue;
                }
                loaded->preferredProvider = QStringLiteral("DirectML");
            }
            else
            {
                loaded->preferredProvider =
                    candidate == QStringLiteral("cpu") || configured == QStringLiteral("cpu") ? QStringLiteral("CPU") : QStringLiteral("CUDA");
            }
            return loaded;
        }
        throw std::runtime_error(QStringLiteral("无法加载 ONNX Runtime（%1）").arg(errors.join(QStringLiteral("；"))).toStdString());
    }

    struct GalleryEmbedding
    {
        QString            styleId;
        std::vector<float> values;
    };

    [[nodiscard]] cv::Mat loadImage(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        const QByteArray bytes = file.readAll();
        if (bytes.size() > std::numeric_limits<int>::max())
            return {};
        cv::Mat encoded(1, static_cast<int>(bytes.size()), CV_8U);
        std::memcpy(encoded.data, bytes.constData(), static_cast<std::size_t>(bytes.size()));
        return cv::imdecode(encoded, cv::IMREAD_COLOR);
    }

    [[nodiscard]] std::vector<float> imageTensor(
        const cv::Mat &bgr, int width, int height, const std::array<float, 3> &mean, const std::array<float, 3> &standardDeviation, bool centerCrop)
    {
        cv::Mat resized;
        if (centerCrop)
        {
            const double scale = std::max(static_cast<double>(width) / bgr.cols, static_cast<double>(height) / bgr.rows);
            cv::resize(bgr,
                       resized,
                       cv::Size(static_cast<int>(std::ceil(bgr.cols * scale)), static_cast<int>(std::ceil(bgr.rows * scale))),
                       0.0,
                       0.0,
                       cv::INTER_CUBIC);
            const int left = (resized.cols - width) / 2;
            const int top  = (resized.rows - height) / 2;
            resized        = resized(cv::Rect(left, top, width, height)).clone();
        }
        else
        {
            cv::resize(bgr, resized, cv::Size(width, height), 0.0, 0.0, cv::INTER_LINEAR);
        }

        cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);
        std::vector<float> tensor(static_cast<std::size_t>(3 * width * height));
        const int          planeSize = width * height;
        for (int y = 0; y < height; ++y)
        {
            const auto *row = resized.ptr<cv::Vec3b>(y);
            for (int x = 0; x < width; ++x)
            {
                for (int channel = 0; channel < 3; ++channel)
                {
                    tensor[static_cast<std::size_t>(channel * planeSize + y * width + x)] =
                        (static_cast<float>(row[x][channel]) / 255.0F - mean[channel]) / standardDeviation[channel];
                }
            }
        }
        return tensor;
    }

    [[nodiscard]] Ort::SessionOptions sessionOptions(const QString &provider, const LoadedOrt &loaded)
    {
        Ort::SessionOptions options;
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        if (provider == QStringLiteral("CUDA"))
        {
            OrtCUDAProviderOptions cudaOptions {};
            options.AppendExecutionProvider_CUDA(cudaOptions);
        }
#ifdef Q_OS_WIN
        else if (provider == QStringLiteral("DirectML"))
        {
            options.DisableMemPattern();
            options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
            Ort::ThrowOnError(loaded.appendDml(options, 0));
        }
#endif
        return options;
    }

    [[nodiscard]] std::unique_ptr<Ort::Session> createSession(Ort::Env        &environment,
                                                              const QString   &path,
                                                              const QString   &provider,
                                                              const LoadedOrt &loaded)
    {
        const Ort::SessionOptions options = sessionOptions(provider, loaded);
#ifdef Q_OS_WIN
        const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
        return std::make_unique<Ort::Session>(environment, nativePath.c_str(), options);
#else
        const QByteArray nativePath = QFile::encodeName(path);
        return std::make_unique<Ort::Session>(environment, nativePath.constData(), options);
#endif
    }

    [[nodiscard]] Runtime createRuntime(const GarmentMatcher::Options &options)
    {
        Runtime       runtime(loadOrtLibrary());
        const QString preferredProvider = runtime.loaded->preferredProvider;
        QStringList   providers {preferredProvider};
        if (preferredProvider != QStringLiteral("CPU"))
            providers.push_back(QStringLiteral("CPU"));

        QStringList errors;
        for (const QString &provider : std::as_const(providers))
        {
            try
            {
                runtime.segmentation = createSession(runtime.environment, options.segmentationModelPath, provider, *runtime.loaded);
                runtime.embedding    = createSession(runtime.environment, options.embeddingModelPath, provider, *runtime.loaded);
                runtime.provider     = provider;
                return runtime;
            }
            catch (const Ort::Exception &error)
            {
                runtime.segmentation.reset();
                runtime.embedding.reset();
                errors.push_back(QStringLiteral("%1: %2").arg(provider, QString::fromUtf8(error.what())));
            }
        }
        throw std::runtime_error(QStringLiteral("无法创建 ONNX Runtime Session（%1）").arg(errors.join(QStringLiteral("；"))).toStdString());
    }

    [[nodiscard]] std::vector<float> encodeImage(Ort::Session &session, const cv::Mat &image)
    {
        constexpr std::array<float, 3> mean {0.48145466F, 0.4578275F, 0.40821073F};
        constexpr std::array<float, 3> standardDeviation {0.26862954F, 0.26130258F, 0.27577711F};
        std::vector<float>             input = imageTensor(image, 224, 224, mean, standardDeviation, true);
        const std::array<int64_t, 4>   shape {1, 3, 224, 224};
        const Ort::MemoryInfo          memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value                     tensor = Ort::Value::CreateTensor<float>(memory, input.data(), input.size(), shape.data(), shape.size());
        constexpr std::array<const char *, 1> inputNames {"pixel_values"};
        constexpr std::array<const char *, 1> outputNames {"image_embeds"};
        auto outputs = session.Run(Ort::RunOptions {nullptr}, inputNames.data(), &tensor, inputNames.size(), outputNames.data(), outputNames.size());
        const auto shapeInfo   = outputs.front().GetTensorTypeAndShapeInfo();
        const auto outputShape = shapeInfo.GetShape();
        if (outputShape.size() != 2 || outputShape[0] != 1 || outputShape[1] != kEmbeddingSize)
            throw std::runtime_error("FashionCLIP image_embeds 输出形状不是 [1,512]");

        const float       *values = outputs.front().GetTensorData<float>();
        std::vector<float> result(values, values + kEmbeddingSize);
        const float        norm = std::sqrt(std::inner_product(result.begin(), result.end(), result.begin(), 0.0F));
        if (norm <= 0.0F)
            throw std::runtime_error("FashionCLIP 返回零向量");
        for (float &value : result)
            value /= norm;
        return result;
    }

    [[nodiscard]] cv::Mat largestMaskComponent(const cv::Mat &mask)
    {
        cv::Mat   labels;
        cv::Mat   statistics;
        cv::Mat   centroids;
        const int count = cv::connectedComponentsWithStats(mask, labels, statistics, centroids, 8, CV_32S);
        if (count <= 1)
            return {};

        int largestLabel = 1;
        for (int label = 2; label < count; ++label)
        {
            if (statistics.at<int>(label, cv::CC_STAT_AREA) > statistics.at<int>(largestLabel, cv::CC_STAT_AREA))
                largestLabel = label;
        }
        if (statistics.at<int>(largestLabel, cv::CC_STAT_AREA) < mask.total() / 200)
            return {};
        cv::Mat result;
        cv::compare(labels, largestLabel, result, cv::CMP_EQ);
        return result;
    }

    [[nodiscard]] cv::Mat maskedCrop(const cv::Mat &image, const cv::Mat &smallMask)
    {
        if (smallMask.empty())
            return {};
        cv::Mat mask;
        cv::resize(smallMask, mask, image.size(), 0.0, 0.0, cv::INTER_NEAREST);
        mask = largestMaskComponent(mask);
        if (mask.empty())
            return {};

        std::vector<cv::Point> points;
        cv::findNonZero(mask, points);
        if (points.empty())
            return {};
        const cv::Rect bounds = cv::boundingRect(points);
        cv::Mat        crop(bounds.size(), image.type(), cv::Scalar(255, 255, 255));
        image(bounds).copyTo(crop, mask(bounds));
        return crop;
    }

    struct GarmentCrops
    {
        cv::Mat upper;
        cv::Mat lower;
    };

    [[nodiscard]] GarmentCrops segmentGarments(Ort::Session &session, const cv::Mat &image)
    {
        constexpr std::array<float, 3> mean {0.485F, 0.456F, 0.406F};
        constexpr std::array<float, 3> standardDeviation {0.229F, 0.224F, 0.225F};
        std::vector<float>             input = imageTensor(image, 512, 512, mean, standardDeviation, false);
        const std::array<int64_t, 4>   inputShape {1, 3, 512, 512};
        const Ort::MemoryInfo          memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value tensor = Ort::Value::CreateTensor<float>(memory, input.data(), input.size(), inputShape.data(), inputShape.size());
        constexpr std::array<const char *, 1> inputNames {"pixel_values"};
        constexpr std::array<const char *, 1> outputNames {"logits"};
        auto outputs = session.Run(Ort::RunOptions {nullptr}, inputNames.data(), &tensor, inputNames.size(), outputNames.data(), outputNames.size());
        const auto outputShape = outputs.front().GetTensorTypeAndShapeInfo().GetShape();
        if (outputShape.size() != 4 || outputShape[0] != 1 || outputShape[1] != 18)
            throw std::runtime_error("服装分割 logits 输出形状不是 [1,18,H,W]");

        const int    height    = static_cast<int>(outputShape[2]);
        const int    width     = static_cast<int>(outputShape[3]);
        const int    planeSize = height * width;
        const float *logits    = outputs.front().GetTensorData<float>();
        cv::Mat      upperMask(height, width, CV_8U, cv::Scalar(0));
        cv::Mat      lowerMask(height, width, CV_8U, cv::Scalar(0));
        cv::Mat      dressMask(height, width, CV_8U, cv::Scalar(0));
        for (int offset = 0; offset < planeSize; ++offset)
        {
            int   bestClass = 0;
            float bestScore = logits[offset];
            for (int classIndex = 1; classIndex < 18; ++classIndex)
            {
                const float score = logits[classIndex * planeSize + offset];
                if (score > bestScore)
                {
                    bestClass = classIndex;
                    bestScore = score;
                }
            }
            const int y = offset / width;
            const int x = offset % width;
            if (bestClass == 4)
                upperMask.at<uchar>(y, x) = 255;
            else if (bestClass == 5 || bestClass == 6)
                lowerMask.at<uchar>(y, x) = 255;
            else if (bestClass == 7)
                dressMask.at<uchar>(y, x) = 255;
        }

        const int dressArea = cv::countNonZero(dressMask);
        if (dressArea > cv::countNonZero(upperMask) + cv::countNonZero(lowerMask))
            return {maskedCrop(image, dressMask), {}};
        return {maskedCrop(image, upperMask), maskedCrop(image, lowerMask)};
    }

    [[nodiscard]] QString modelKey(const QString &path)
    {
        const QFileInfo info(path);
        return QStringLiteral("%1:%2").arg(info.size()).arg(info.lastModified().toMSecsSinceEpoch());
    }

    void executeSql(sqlite3 *database, const char *sql)
    {
        char *error = nullptr;
        if (sqlite3_exec(database, sql, nullptr, nullptr, &error) != SQLITE_OK)
        {
            const QString message = QString::fromUtf8(error ? error : "SQLite error");
            sqlite3_free(error);
            throw std::runtime_error(message.toStdString());
        }
    }

    [[nodiscard]] Database openDatabase(const QString &path)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        sqlite3         *rawDatabase = nullptr;
        const QByteArray encodedPath = path.toUtf8();
        if (sqlite3_open(encodedPath.constData(), &rawDatabase) != SQLITE_OK)
        {
            const QString message = rawDatabase ? QString::fromUtf8(sqlite3_errmsg(rawDatabase)) : QStringLiteral("无法创建 SQLite 特征库");
            if (rawDatabase)
                sqlite3_close(rawDatabase);
            throw std::runtime_error(message.toStdString());
        }
        Database database(rawDatabase);
        executeSql(database.get(),
                   "CREATE TABLE IF NOT EXISTS image_embeddings("
                   "path TEXT PRIMARY KEY, style_id TEXT NOT NULL, file_size INTEGER NOT NULL, "
                   "modified_ms INTEGER NOT NULL, model_key TEXT NOT NULL, embedding BLOB NOT NULL)");
        return database;
    }

    [[nodiscard]] Statement prepare(sqlite3 *database, const char *sql)
    {
        sqlite3_stmt *rawStatement = nullptr;
        if (sqlite3_prepare_v2(database, sql, -1, &rawStatement, nullptr) != SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(database));
        return Statement(rawStatement);
    }

    [[nodiscard]] std::optional<std::vector<float>> cachedEmbedding(sqlite3 *database, const GalleryItem &item, const QString &embeddingModelKey)
    {
        const QFileInfo  info(item.imagePath);
        Statement        statement = prepare(database,
                                             "SELECT embedding FROM image_embeddings WHERE path=?1 AND style_id=?2 "
                                             "AND file_size=?3 AND modified_ms=?4 AND model_key=?5");
        const QByteArray path      = item.imagePath.toUtf8();
        const QByteArray styleId   = item.styleId.toUtf8();
        const QByteArray key       = embeddingModelKey.toUtf8();
        sqlite3_bind_text(statement.get(), 1, path.constData(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement.get(), 2, styleId.constData(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(statement.get(), 3, info.size());
        sqlite3_bind_int64(statement.get(), 4, info.lastModified().toMSecsSinceEpoch());
        sqlite3_bind_text(statement.get(), 5, key.constData(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(statement.get()) != SQLITE_ROW)
            return std::nullopt;
        const int bytes = sqlite3_column_bytes(statement.get(), 0);
        if (bytes != kEmbeddingSize * static_cast<int>(sizeof(float)))
            return std::nullopt;
        std::vector<float> result(kEmbeddingSize);
        std::memcpy(result.data(), sqlite3_column_blob(statement.get(), 0), static_cast<std::size_t>(bytes));
        return result;
    }

    void storeEmbedding(sqlite3 *database, const GalleryItem &item, const QString &embeddingModelKey, const std::vector<float> &embedding)
    {
        const QFileInfo  info(item.imagePath);
        Statement        statement = prepare(database,
                                             "INSERT OR REPLACE INTO image_embeddings"
                                             "(path,style_id,file_size,modified_ms,model_key,embedding) VALUES(?1,?2,?3,?4,?5,?6)");
        const QByteArray path      = item.imagePath.toUtf8();
        const QByteArray styleId   = item.styleId.toUtf8();
        const QByteArray key       = embeddingModelKey.toUtf8();
        sqlite3_bind_text(statement.get(), 1, path.constData(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement.get(), 2, styleId.constData(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(statement.get(), 3, info.size());
        sqlite3_bind_int64(statement.get(), 4, info.lastModified().toMSecsSinceEpoch());
        sqlite3_bind_text(statement.get(), 5, key.constData(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_blob(statement.get(), 6, embedding.data(), static_cast<int>(embedding.size() * sizeof(float)), SQLITE_TRANSIENT);
        if (sqlite3_step(statement.get()) != SQLITE_DONE)
            throw std::runtime_error(sqlite3_errmsg(database));
    }

    [[nodiscard]] QVector<GalleryEmbedding> galleryEmbeddings(Ort::Session               &session,
                                                              const QVector<GalleryItem> &items,
                                                              sqlite3                    *database,
                                                              const QString              &embeddingModelKey)
    {
        QVector<GalleryEmbedding> result;
        result.reserve(items.size());
        for (const GalleryItem &item : items)
        {
            std::optional<std::vector<float>> embedding = cachedEmbedding(database, item, embeddingModelKey);
            if (!embedding)
            {
                const cv::Mat image = loadImage(item.imagePath);
                if (image.empty())
                    continue;
                embedding = encodeImage(session, image);
                storeEmbedding(database, item, embeddingModelKey, *embedding);
            }
            result.push_back({item.styleId, std::move(*embedding)});
        }
        return result;
    }

    [[nodiscard]] GarmentMatcher::Match bestMatch(const std::vector<float> &query, const QVector<GalleryEmbedding> &gallery)
    {
        GarmentMatcher::Match result;
        result.score = -1.0F;
        for (const GalleryEmbedding &candidate : gallery)
        {
            const float score = std::inner_product(query.begin(), query.end(), candidate.values.begin(), 0.0F);
            if (score > result.score)
            {
                result.styleId = candidate.styleId;
                result.score   = score;
            }
        }
        return result;
    }

} // namespace

QStringList GarmentMatcher::availableProviders()
{
    QStringList providers;
#ifdef Q_OS_WIN
    if (hasCudaDriver() && QFileInfo::exists(runtimeLibraryPath(QStringLiteral("cuda"))))
        providers.push_back(QStringLiteral("CUDA"));
    if (QFileInfo::exists(runtimeLibraryPath(QStringLiteral("directml"))))
        providers.push_back(QStringLiteral("DirectML"));
#endif
    providers.push_back(QStringLiteral("CPU"));
    return providers;
}

QString GarmentMatcher::Result::joinedStyleIds() const
{
    QStringList styleIds;
    if (!upper.styleId.isEmpty())
        styleIds.push_back(upper.styleId);
    if (!lower.styleId.isEmpty() && lower.styleId != upper.styleId)
        styleIds.push_back(lower.styleId);
    return styleIds.join(QLatin1Char(','));
}

GarmentMatcher::Result GarmentMatcher::match(const QString &photoPath, const QVector<GalleryItem> &galleryItems, const Options &options)
{
    Result result;
    try
    {
        if (!QFileInfo::exists(photoPath))
            throw std::runtime_error("当前实拍图不存在");
        if (galleryItems.isEmpty())
            throw std::runtime_error("款号小图库为空");
        if (!QFileInfo::exists(options.segmentationModelPath))
            throw std::runtime_error("服装分割模型不存在");
        if (!QFileInfo::exists(options.embeddingModelPath))
            throw std::runtime_error("FashionCLIP 图像模型不存在");

        cv::Mat photo = loadImage(photoPath);
        if (photo.empty())
            throw std::runtime_error("无法读取当前实拍图");

        Runtime runtime    = createRuntime(options);
        result.provider    = runtime.provider;
        GarmentCrops crops = segmentGarments(*runtime.segmentation, photo);
        if (crops.upper.empty() && crops.lower.empty())
            throw std::runtime_error("没有分割出上衣、裤子、裙子或连衣裙");

        Database                        database = openDatabase(options.featureDatabasePath);
        const QVector<GalleryEmbedding> gallery =
            galleryEmbeddings(*runtime.embedding, galleryItems, database.get(), modelKey(options.embeddingModelPath));
        if (gallery.isEmpty())
            throw std::runtime_error("款号小图库中没有可读取的图片");

        if (!crops.upper.empty())
            result.upper = bestMatch(encodeImage(*runtime.embedding, crops.upper), gallery);
        if (!crops.lower.empty())
            result.lower = bestMatch(encodeImage(*runtime.embedding, crops.lower), gallery);
        result.success = true;
    }
    catch (const std::exception &error)
    {
        result.error = QString::fromUtf8(error.what());
    }
    return result;
}
