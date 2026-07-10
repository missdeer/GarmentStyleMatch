#include <algorithm>
#include <cmath>
#include <archive.h>
#include <archive_entry.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QXmlStreamReader>

#include "PptStyleExtractor.h"

namespace
{

    struct Box
    {
        double x     = 0.0;
        double y     = 0.0;
        double cx    = 0.0;
        double cy    = 0.0;
        bool   valid = false;

        double centerX() const
        {
            return x + cx / 2.0;
        }
        double centerY() const
        {
            return y + cy / 2.0;
        }
        double right() const
        {
            return x + cx;
        }
        double bottom() const
        {
            return y + cy;
        }
    };

    struct RawTransform
    {
        Box    outer;
        double childX      = 0.0;
        double childY      = 0.0;
        double childCx     = 0.0;
        double childCy     = 0.0;
        bool   hasChildBox = false;
    };

    struct GroupTransform
    {
        RawTransform value;

        Box map(const Box &box) const
        {
            if (!box.valid || !value.outer.valid || !value.hasChildBox || value.childCx == 0.0 || value.childCy == 0.0)
            {
                return box;
            }

            Box result;
            result.x     = value.outer.x + (box.x - value.childX) * value.outer.cx / value.childCx;
            result.y     = value.outer.y + (box.y - value.childY) * value.outer.cy / value.childCy;
            result.cx    = box.cx * value.outer.cx / value.childCx;
            result.cy    = box.cy * value.outer.cy / value.childCy;
            result.valid = true;
            return result;
        }
    };

    struct TextShape
    {
        Box     box;
        QString text;
    };

    struct PictureShape
    {
        Box     box;
        QString relationshipId;
    };

    struct TableCell
    {
        int     column     = 0;
        int     columnSpan = 1;
        QString text;
    };

    struct TableRow
    {
        double             height = 0.0;
        QVector<TableCell> cells;
    };

    struct TableData
    {
        QVector<double>   columnWidths;
        QVector<TableRow> rows;
    };

    struct SlideContents
    {
        QVector<TextShape>    texts;
        QVector<PictureShape> pictures;
    };

    struct PresentationInfo
    {
        QVector<QString> slideParts;
        double           width  = 0.0;
        double           height = 0.0;
    };

    struct PendingStyle
    {
        QString          styleId;
        int              sourcePage = 0;
        QVector<QString> mediaParts;
    };

    QString archiveError(struct archive *value)
    {
        const char *message = archive_error_string(value);
        return message ? QString::fromUtf8(message) : QStringLiteral("未知错误");
    }

    bool isSafeRelativePath(const QString &path)
    {
        if (path.isEmpty() || QDir::isAbsolutePath(path))
            return false;
        if (path == QLatin1String("..") || path.startsWith(QLatin1String("../")))
            return false;
        return !QRegularExpression(QStringLiteral("^[A-Za-z]:")).match(path).hasMatch();
    }

    bool recreateDirectory(const QString &path, QStringList &warnings)
    {
        const QString absolutePath = QFileInfo(path).absoluteFilePath();
        if (absolutePath.isEmpty() || QDir(absolutePath).isRoot())
        {
            warnings << QStringLiteral("拒绝清理不安全的缓存目录: %1").arg(path);
            return false;
        }

        QDir directory(absolutePath);
        if (directory.exists() && !directory.removeRecursively())
        {
            warnings << QStringLiteral("无法清理缓存目录: %1").arg(absolutePath);
            return false;
        }
        if (!QDir().mkpath(absolutePath))
        {
            warnings << QStringLiteral("无法创建缓存目录: %1").arg(absolutePath);
            return false;
        }
        return true;
    }

    bool unpackPptx(const QString &pptxPath, const QString &openXmlDir, QStringList &warnings)
    {
        if (!recreateDirectory(openXmlDir, warnings))
            return false;

        struct archive *reader = archive_read_new();
        archive_read_support_format_zip(reader);

#ifdef Q_OS_WIN
        const std::wstring nativePath = QDir::toNativeSeparators(pptxPath).toStdWString();
        const int          openResult = archive_read_open_filename_w(reader, nativePath.c_str(), 64 * 1024);
#else
        const QByteArray nativePath = QFile::encodeName(pptxPath);
        const int        openResult = archive_read_open_filename(reader, nativePath.constData(), 64 * 1024);
#endif
        if (openResult != ARCHIVE_OK)
        {
            warnings << QStringLiteral("无法按 ZIP 打开 PPTX: %1").arg(archiveError(reader));
            archive_read_free(reader);
            return false;
        }

        int                   extractedFiles = 0;
        struct archive_entry *entry          = nullptr;
        int                   headerResult   = ARCHIVE_OK;
        while ((headerResult = archive_read_next_header(reader, &entry)) == ARCHIVE_OK)
        {
            const char *utf8Name  = archive_entry_pathname_utf8(entry);
            QString     entryName = utf8Name ? QString::fromUtf8(utf8Name) : QString::fromLocal8Bit(archive_entry_pathname(entry));
            entryName.replace(QLatin1Char('\\'), QLatin1Char('/'));
            entryName = QDir::cleanPath(entryName);

            if (!isSafeRelativePath(entryName))
            {
                warnings << QStringLiteral("跳过不安全的 ZIP 条目: %1").arg(entryName);
                archive_read_data_skip(reader);
                continue;
            }

            const QString outputPath = QDir(openXmlDir).absoluteFilePath(QDir::toNativeSeparators(entryName));
            const auto    fileType   = archive_entry_filetype(entry);
            if (fileType == AE_IFDIR)
            {
                if (!QDir().mkpath(outputPath))
                {
                    warnings << QStringLiteral("无法创建解压目录: %1").arg(outputPath);
                    archive_read_free(reader);
                    return false;
                }
                continue;
            }
            if (fileType != AE_IFREG)
            {
                archive_read_data_skip(reader);
                continue;
            }

            if (!QDir().mkpath(QFileInfo(outputPath).absolutePath()))
            {
                warnings << QStringLiteral("无法创建解压目录: %1").arg(QFileInfo(outputPath).absolutePath());
                archive_read_free(reader);
                return false;
            }

            QSaveFile output(outputPath);
            if (!output.open(QIODevice::WriteOnly))
            {
                warnings << QStringLiteral("无法写入解压文件: %1").arg(outputPath);
                archive_read_free(reader);
                return false;
            }

            char buffer[64 * 1024];
            for (;;)
            {
                const la_ssize_t count = archive_read_data(reader, buffer, sizeof(buffer));
                if (count < 0)
                {
                    warnings << QStringLiteral("解压 %1 失败: %2").arg(entryName, archiveError(reader));
                    archive_read_free(reader);
                    return false;
                }
                if (count == 0)
                    break;
                if (output.write(buffer, count) != count)
                {
                    warnings << QStringLiteral("写入解压文件失败: %1").arg(outputPath);
                    archive_read_free(reader);
                    return false;
                }
            }
            if (!output.commit())
            {
                warnings << QStringLiteral("提交解压文件失败: %1").arg(outputPath);
                archive_read_free(reader);
                return false;
            }
            ++extractedFiles;
        }

        if (headerResult != ARCHIVE_EOF)
        {
            warnings << QStringLiteral("读取 PPTX ZIP 失败: %1").arg(archiveError(reader));
            archive_read_free(reader);
            return false;
        }

        archive_read_free(reader);
        warnings << QStringLiteral("已将 PPTX 解压到 Open XML 缓存（%1 个文件）: %2").arg(extractedFiles).arg(QDir::toNativeSeparators(openXmlDir));
        return true;
    }

    QByteArray readPart(const QString &openXmlDir, const QString &partName, QStringList &warnings)
    {
        QFile file(QDir(openXmlDir).absoluteFilePath(QDir::toNativeSeparators(partName)));
        if (!file.open(QIODevice::ReadOnly))
        {
            warnings << QStringLiteral("无法读取 Open XML 部件: %1").arg(partName);
            return {};
        }
        return file.readAll();
    }

    QString relationshipPartName(const QString &sourcePart)
    {
        const QFileInfo source(sourcePart);
        return QDir::cleanPath(source.path() + QStringLiteral("/_rels/") + source.fileName() + QStringLiteral(".rels"));
    }

    QString resolveRelationshipTarget(const QString &sourcePart, QString target)
    {
        target.replace(QLatin1Char('\\'), QLatin1Char('/'));
        while (target.startsWith(QLatin1Char('/')))
            target.remove(0, 1);

        const QString resolved = QDir::cleanPath(QFileInfo(sourcePart).path() + QLatin1Char('/') + target);
        return isSafeRelativePath(resolved) ? resolved : QString();
    }

    QMap<QString, QString> readRelationships(const QString &openXmlDir, const QString &sourcePart, const QString &typeSuffix, QStringList &warnings)
    {
        QMap<QString, QString> relationships;
        const QByteArray       xml = readPart(openXmlDir, relationshipPartName(sourcePart), warnings);
        if (xml.isEmpty())
            return relationships;

        QXmlStreamReader reader(xml);
        while (!reader.atEnd())
        {
            reader.readNext();
            if (!reader.isStartElement() || reader.name() != QLatin1String("Relationship"))
            {
                continue;
            }

            const auto    attributes = reader.attributes();
            const QString id         = attributes.value(QLatin1String("Id")).toString();
            const QString type       = attributes.value(QLatin1String("Type")).toString();
            const QString target     = attributes.value(QLatin1String("Target")).toString();
            const QString targetMode = attributes.value(QLatin1String("TargetMode")).toString();
            if (id.isEmpty() || target.isEmpty() || targetMode.compare(QLatin1String("External"), Qt::CaseInsensitive) == 0 ||
                !type.endsWith(typeSuffix))
            {
                continue;
            }

            const QString resolved = resolveRelationshipTarget(sourcePart, target);
            if (resolved.isEmpty())
            {
                warnings << QStringLiteral("忽略越界的关系目标: %1").arg(target);
                continue;
            }
            relationships.insert(id, resolved);
        }
        if (reader.hasError())
        {
            warnings << QStringLiteral("关系 XML 解析失败 (%1): %2").arg(sourcePart, reader.errorString());
        }
        return relationships;
    }

    QString relationshipId(const QXmlStreamAttributes &attributes, QStringView localName)
    {
        for (const auto &attribute : attributes)
        {
            if (attribute.name() != localName)
                continue;
            if (attribute.namespaceUri().contains(QLatin1String("relationships")) || attribute.qualifiedName().contains(QLatin1Char(':')))
            {
                return attribute.value().toString();
            }
        }
        return {};
    }

    PresentationInfo readPresentation(const QString &openXmlDir, QStringList &warnings)
    {
        PresentationInfo             result;
        const QString                presentationPart   = QStringLiteral("ppt/presentation.xml");
        const QMap<QString, QString> slideRelationships = readRelationships(openXmlDir, presentationPart, QStringLiteral("/slide"), warnings);
        const QByteArray             xml                = readPart(openXmlDir, presentationPart, warnings);
        if (xml.isEmpty())
            return result;

        QXmlStreamReader reader(xml);
        while (!reader.atEnd())
        {
            reader.readNext();
            if (!reader.isStartElement())
                continue;
            if (reader.name() == QLatin1String("sldId"))
            {
                const QString id   = relationshipId(reader.attributes(), QStringLiteral("id"));
                const QString part = slideRelationships.value(id);
                if (!part.isEmpty())
                    result.slideParts.push_back(part);
            }
            else if (reader.name() == QLatin1String("sldSz"))
            {
                result.width  = reader.attributes().value(QLatin1String("cx")).toDouble();
                result.height = reader.attributes().value(QLatin1String("cy")).toDouble();
            }
        }
        if (reader.hasError())
        {
            warnings << QStringLiteral("presentation.xml 解析失败: %1").arg(reader.errorString());
        }
        return result;
    }

    RawTransform readTransform(QXmlStreamReader &reader)
    {
        RawTransform result;
        while (!reader.atEnd())
        {
            reader.readNext();
            if (reader.isEndElement() && reader.name() == QLatin1String("xfrm"))
                break;
            if (!reader.isStartElement())
                continue;

            const auto attributes = reader.attributes();
            if (reader.name() == QLatin1String("off"))
            {
                result.outer.x = attributes.value(QLatin1String("x")).toDouble();
                result.outer.y = attributes.value(QLatin1String("y")).toDouble();
            }
            else if (reader.name() == QLatin1String("ext"))
            {
                result.outer.cx    = attributes.value(QLatin1String("cx")).toDouble();
                result.outer.cy    = attributes.value(QLatin1String("cy")).toDouble();
                result.outer.valid = result.outer.cx > 0.0 && result.outer.cy > 0.0;
            }
            else if (reader.name() == QLatin1String("chOff"))
            {
                result.childX      = attributes.value(QLatin1String("x")).toDouble();
                result.childY      = attributes.value(QLatin1String("y")).toDouble();
                result.hasChildBox = true;
            }
            else if (reader.name() == QLatin1String("chExt"))
            {
                result.childCx     = attributes.value(QLatin1String("cx")).toDouble();
                result.childCy     = attributes.value(QLatin1String("cy")).toDouble();
                result.hasChildBox = true;
            }
        }
        return result;
    }

    QString readTextBody(QXmlStreamReader &reader, QLatin1String endElement)
    {
        QString text;
        bool    hasParagraph = false;
        while (!reader.atEnd())
        {
            reader.readNext();
            if (reader.isEndElement() && reader.name() == endElement)
                break;
            if (!reader.isStartElement())
                continue;
            if (reader.name() == QLatin1String("p"))
            {
                if (hasParagraph)
                    text += QLatin1Char('\n');
                hasParagraph = true;
            }
            else if (reader.name() == QLatin1String("t"))
            {
                text += reader.readElementText();
            }
        }
        return text;
    }

    Box applyGroupTransforms(Box box, const QVector<GroupTransform> &transforms)
    {
        for (auto iterator = transforms.crbegin(); iterator != transforms.crend(); ++iterator)
            box = iterator->map(box);
        return box;
    }

    TextShape readTextShape(QXmlStreamReader &reader, const QVector<GroupTransform> &transforms)
    {
        TextShape result;
        while (!reader.atEnd())
        {
            reader.readNext();
            if (reader.isEndElement() && reader.name() == QLatin1String("sp"))
                break;
            if (!reader.isStartElement())
                continue;
            if (reader.name() == QLatin1String("xfrm"))
                result.box = readTransform(reader).outer;
            else if (reader.name() == QLatin1String("txBody"))
                result.text = readTextBody(reader, QLatin1String("txBody"));
        }
        result.box = applyGroupTransforms(result.box, transforms);
        return result;
    }

    PictureShape readPictureShape(QXmlStreamReader &reader, const QVector<GroupTransform> &transforms)
    {
        PictureShape result;
        while (!reader.atEnd())
        {
            reader.readNext();
            if (reader.isEndElement() && reader.name() == QLatin1String("pic"))
                break;
            if (!reader.isStartElement())
                continue;
            if (reader.name() == QLatin1String("xfrm"))
            {
                result.box = readTransform(reader).outer;
            }
            else if (reader.name() == QLatin1String("blip"))
            {
                result.relationshipId = relationshipId(reader.attributes(), QStringLiteral("embed"));
            }
        }
        result.box = applyGroupTransforms(result.box, transforms);
        return result;
    }

    TableCell readTableCell(QXmlStreamReader &reader, int column)
    {
        TableCell result;
        result.column    = column;
        bool      spanOk = false;
        const int span   = reader.attributes().value(QLatin1String("gridSpan")).toInt(&spanOk);
        if (spanOk && span > 1)
            result.columnSpan = span;
        result.text = readTextBody(reader, QLatin1String("tc"));
        return result;
    }

    TableRow readTableRow(QXmlStreamReader &reader)
    {
        TableRow result;
        result.height = reader.attributes().value(QLatin1String("h")).toDouble();
        int column    = 0;
        while (!reader.atEnd())
        {
            reader.readNext();
            if (reader.isEndElement() && reader.name() == QLatin1String("tr"))
                break;
            if (!reader.isStartElement() || reader.name() != QLatin1String("tc"))
                continue;
            TableCell cell = readTableCell(reader, column);
            column += cell.columnSpan;
            result.cells.push_back(std::move(cell));
        }
        return result;
    }

    TableData readTable(QXmlStreamReader &reader)
    {
        TableData result;
        while (!reader.atEnd())
        {
            reader.readNext();
            if (reader.isEndElement() && reader.name() == QLatin1String("tbl"))
                break;
            if (!reader.isStartElement())
                continue;
            if (reader.name() == QLatin1String("gridCol"))
            {
                result.columnWidths.push_back(reader.attributes().value(QLatin1String("w")).toDouble());
            }
            else if (reader.name() == QLatin1String("tr"))
            {
                result.rows.push_back(readTableRow(reader));
            }
        }
        return result;
    }

    void appendTableTexts(const Box &frame, const TableData &table, const QVector<GroupTransform> &transforms, QVector<TextShape> &texts)
    {
        if (!frame.valid || table.columnWidths.isEmpty() || table.rows.isEmpty())
            return;

        double tableWidth = 0.0;
        for (double width : table.columnWidths)
            tableWidth += width;
        double tableHeight = 0.0;
        for (const auto &row : table.rows)
            tableHeight += row.height;
        if (tableWidth <= 0.0 || tableHeight <= 0.0)
            return;

        QVector<double> columnOffsets(table.columnWidths.size() + 1, 0.0);
        for (int i = 0; i < table.columnWidths.size(); ++i)
            columnOffsets[i + 1] = columnOffsets[i] + table.columnWidths[i];

        double rowOffset = 0.0;
        for (const auto &row : table.rows)
        {
            for (const auto &cell : row.cells)
            {
                if (cell.text.trimmed().isEmpty() || cell.column < 0 || cell.column >= table.columnWidths.size())
                {
                    continue;
                }
                const int endColumn = std::min(cell.column + cell.columnSpan, int(table.columnWidths.size()));
                Box       cellBox;
                cellBox.x     = frame.x + columnOffsets[cell.column] * frame.cx / tableWidth;
                cellBox.y     = frame.y + rowOffset * frame.cy / tableHeight;
                cellBox.cx    = (columnOffsets[endColumn] - columnOffsets[cell.column]) * frame.cx / tableWidth;
                cellBox.cy    = row.height * frame.cy / tableHeight;
                cellBox.valid = cellBox.cx > 0.0 && cellBox.cy > 0.0;
                texts.push_back({applyGroupTransforms(cellBox, transforms), cell.text});
            }
            rowOffset += row.height;
        }
    }

    void readGraphicFrame(QXmlStreamReader &reader, const QVector<GroupTransform> &transforms, QVector<TextShape> &texts)
    {
        Box       frame;
        TableData table;
        while (!reader.atEnd())
        {
            reader.readNext();
            if (reader.isEndElement() && reader.name() == QLatin1String("graphicFrame"))
            {
                break;
            }
            if (!reader.isStartElement())
                continue;
            if (reader.name() == QLatin1String("xfrm"))
                frame = readTransform(reader).outer;
            else if (reader.name() == QLatin1String("tbl"))
                table = readTable(reader);
        }
        appendTableTexts(frame, table, transforms, texts);
    }

    GroupTransform readGroupProperties(QXmlStreamReader &reader)
    {
        GroupTransform result;
        while (!reader.atEnd())
        {
            reader.readNext();
            if (reader.isEndElement() && reader.name() == QLatin1String("grpSpPr"))
                break;
            if (reader.isStartElement() && reader.name() == QLatin1String("xfrm"))
                result.value = readTransform(reader);
        }
        return result;
    }

    void readShapeContainer(QXmlStreamReader              &reader,
                            QLatin1String                  endElement,
                            const QVector<GroupTransform> &parentTransforms,
                            SlideContents                 &contents)
    {
        QVector<GroupTransform> transforms = parentTransforms;
        while (!reader.atEnd())
        {
            reader.readNext();
            if (reader.isEndElement() && reader.name() == endElement)
                break;
            if (!reader.isStartElement())
                continue;

            if (reader.name() == QLatin1String("grpSpPr") && endElement == QLatin1String("grpSp"))
            {
                const GroupTransform transform = readGroupProperties(reader);
                if (transform.value.outer.valid && transform.value.hasChildBox)
                    transforms.push_back(transform);
            }
            else if (reader.name() == QLatin1String("sp"))
            {
                TextShape text = readTextShape(reader, transforms);
                if (!text.text.trimmed().isEmpty() && text.box.valid)
                    contents.texts.push_back(std::move(text));
            }
            else if (reader.name() == QLatin1String("pic"))
            {
                PictureShape picture = readPictureShape(reader, transforms);
                if (!picture.relationshipId.isEmpty() && picture.box.valid)
                    contents.pictures.push_back(std::move(picture));
            }
            else if (reader.name() == QLatin1String("graphicFrame"))
            {
                readGraphicFrame(reader, transforms, contents.texts);
            }
            else if (reader.name() == QLatin1String("grpSp"))
            {
                readShapeContainer(reader, QLatin1String("grpSp"), transforms, contents);
            }
        }
    }

    SlideContents readSlide(const QByteArray &xml, QStringList &warnings, int page)
    {
        SlideContents    result;
        QXmlStreamReader reader(xml);
        while (!reader.atEnd())
        {
            reader.readNext();
            if (reader.isStartElement() && reader.name() == QLatin1String("spTree"))
            {
                readShapeContainer(reader, QLatin1String("spTree"), {}, result);
                break;
            }
        }
        if (reader.hasError())
        {
            warnings << QStringLiteral("第 %1 页 slide XML 解析失败: %2").arg(page).arg(reader.errorString());
        }
        return result;
    }

    bool looksLikeStyleId(const QString &candidate)
    {
        if (candidate.size() < 6 || candidate.size() > 20)
            return false;

        bool hasLetter = false;
        bool hasDigit  = false;
        for (const QChar character : candidate)
        {
            if (character >= QLatin1Char('A') && character <= QLatin1Char('Z'))
                hasLetter = true;
            else if (character.isDigit())
                hasDigit = true;
            else
                return false;
        }
        return hasLetter && hasDigit;
    }

    QString styleIdFromText(const QString &text)
    {
        static const QRegularExpression token(QStringLiteral("[A-Z0-9]{6,20}"));
        const QStringList               lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines)
        {
            QString compact;
            compact.reserve(line.size());
            for (const QChar character : line)
            {
                if (!character.isSpace())
                    compact += character;
            }
            if (looksLikeStyleId(compact))
                return compact;

            auto iterator = token.globalMatch(line);
            while (iterator.hasNext())
            {
                const QString candidate = iterator.next().captured();
                if (looksLikeStyleId(candidate))
                    return candidate;
            }
        }
        return {};
    }

    struct Label
    {
        QString          styleId;
        Box              box;
        QVector<QString> pictureIds;
    };

    struct LabelRow
    {
        QVector<int> labels;
        double       centerY   = 0.0;
        double       maxHeight = 0.0;
    };

    double overlapLength(double firstStart, double firstEnd, double secondStart, double secondEnd)
    {
        return std::max(0.0, std::min(firstEnd, secondEnd) - std::max(firstStart, secondStart));
    }

    QVector<LabelRow> groupLabelRows(const QVector<Label> &labels)
    {
        QVector<int> order(labels.size());
        for (int index = 0; index < labels.size(); ++index)
            order[index] = index;
        std::sort(order.begin(), order.end(), [&labels](int left, int right) {
            if (labels[left].box.centerY() == labels[right].box.centerY())
                return labels[left].box.centerX() < labels[right].box.centerX();
            return labels[left].box.centerY() < labels[right].box.centerY();
        });

        QVector<LabelRow> rows;
        for (int labelIndex : order)
        {
            const Box &box = labels[labelIndex].box;
            if (rows.isEmpty() || std::abs(box.centerY() - rows.back().centerY) > std::max(box.cy, rows.back().maxHeight) * 0.6)
            {
                rows.push_back({{labelIndex}, box.centerY(), box.cy});
                continue;
            }

            LabelRow &row = rows.back();
            row.labels.push_back(labelIndex);
            const int count = row.labels.size();
            row.centerY     = (row.centerY * (count - 1) + box.centerY()) / count;
            row.maxHeight   = std::max(row.maxHeight, box.cy);
        }

        for (LabelRow &row : rows)
        {
            std::sort(row.labels.begin(), row.labels.end(), [&labels](int left, int right) {
                return labels[left].box.centerX() < labels[right].box.centerX();
            });
        }
        return rows;
    }

    QVector<QPair<QString, QVector<QString>>> assignPicturesToLabels(const SlideContents &contents,
                                                                     double               slideWidth,
                                                                     double               slideHeight,
                                                                     int                 &unassignedPictures)
    {
        QVector<Label> labels;
        for (const auto &text : contents.texts)
        {
            const QString styleId = styleIdFromText(text.text);
            if (!styleId.isEmpty())
                labels.push_back({styleId, text.box, {}});
        }
        if (labels.isEmpty())
        {
            unassignedPictures = contents.pictures.size();
            return {};
        }

        if (slideWidth <= 0.0)
        {
            for (const auto &label : labels)
                slideWidth = std::max(slideWidth, label.box.right());
            for (const auto &picture : contents.pictures)
                slideWidth = std::max(slideWidth, picture.box.right());
        }
        if (slideHeight <= 0.0)
        {
            for (const auto &label : labels)
                slideHeight = std::max(slideHeight, label.box.bottom());
            for (const auto &picture : contents.pictures)
                slideHeight = std::max(slideHeight, picture.box.bottom());
        }

        const QVector<LabelRow> rows = groupLabelRows(labels);
        unassignedPictures           = 0;
        for (const auto &picture : contents.pictures)
        {
            int    bestLabel   = -1;
            double bestOverlap = 0.0;

            for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
            {
                const LabelRow &row    = rows[rowIndex];
                const double    top    = row.centerY;
                const double    bottom = rowIndex + 1 < rows.size() ? rows[rowIndex + 1].centerY : slideHeight;
                if (picture.box.centerY() < top || picture.box.centerY() >= bottom)
                    continue;

                for (int column = 0; column < row.labels.size(); ++column)
                {
                    const int    labelIndex = row.labels[column];
                    const double center     = labels[labelIndex].box.centerX();
                    double       left       = labels[labelIndex].box.x;
                    double       right      = labels[labelIndex].box.right();
                    if (column > 0)
                    {
                        left = (labels[row.labels[column - 1]].box.centerX() + center) / 2.0;
                    }
                    else if (row.labels.size() > 1)
                    {
                        const double nextCenter = labels[row.labels[1]].box.centerX();
                        left                    = std::min(left, center - (nextCenter - center) / 2.0);
                    }
                    if (column + 1 < row.labels.size())
                    {
                        right = (center + labels[row.labels[column + 1]].box.centerX()) / 2.0;
                    }
                    else if (row.labels.size() > 1)
                    {
                        const double previousCenter = labels[row.labels[column - 1]].box.centerX();
                        right                       = std::max(right, center + (center - previousCenter) / 2.0);
                    }
                    left  = std::max(0.0, left);
                    right = std::min(slideWidth, right);
                    if (picture.box.centerX() < left || picture.box.centerX() >= right)
                        continue;

                    const double horizontalFraction = overlapLength(picture.box.x, picture.box.right(), left, right) / picture.box.cx;
                    const double verticalFraction   = overlapLength(picture.box.y, picture.box.bottom(), top, bottom) / picture.box.cy;
                    const double overlap            = horizontalFraction * verticalFraction;
                    if (horizontalFraction > 0.5 && verticalFraction > 0.5 && overlap > bestOverlap)
                    {
                        bestLabel   = labelIndex;
                        bestOverlap = overlap;
                    }
                }
            }

            if (bestLabel < 0)
            {
                ++unassignedPictures;
                continue;
            }
            QVector<QString> &ids = labels[bestLabel].pictureIds;
            if (!ids.contains(picture.relationshipId))
                ids.push_back(picture.relationshipId);
        }

        QVector<QPair<QString, QVector<QString>>> result;
        for (const LabelRow &row : rows)
        {
            for (int labelIndex : row.labels)
            {
                const Label &label = labels[labelIndex];
                if (!label.pictureIds.isEmpty())
                    result.push_back({label.styleId, label.pictureIds});
            }
        }
        return result;
    }

    QString sanitizeDirectoryName(QString value)
    {
        static const QRegularExpression invalid(QStringLiteral("[\\\\/:*?\"<>|]"));
        value.replace(invalid, QStringLiteral("_"));
        while (value.endsWith(QLatin1Char('.')) || value.endsWith(QLatin1Char(' ')))
            value.chop(1);
        return value.isEmpty() ? QStringLiteral("unknown-style") : value;
    }

    bool writeExtractedStyles(const QString               &openXmlDir,
                              const QString               &outputDir,
                              const QVector<PendingStyle> &pendingStyles,
                              QVector<ExtractedStyle>     &styles,
                              QStringList                 &warnings)
    {
        if (!recreateDirectory(outputDir, warnings))
            return false;

        for (const PendingStyle &pending : pendingStyles)
        {
            ExtractedStyle extracted;
            extracted.styleId    = pending.styleId;
            extracted.sourcePage = pending.sourcePage;

            const QString styleDir = QDir(outputDir).absoluteFilePath(sanitizeDirectoryName(pending.styleId));
            if (!QDir().mkpath(styleDir))
            {
                warnings << QStringLiteral("无法创建款式缓存目录: %1").arg(styleDir);
                continue;
            }

            int imageIndex = 1;
            for (const QString &mediaPart : pending.mediaParts)
            {
                QFile source(QDir(openXmlDir).absoluteFilePath(QDir::toNativeSeparators(mediaPart)));
                if (!source.open(QIODevice::ReadOnly))
                {
                    warnings << QStringLiteral("无法读取图片部件: %1").arg(mediaPart);
                    continue;
                }

                QString extension = QFileInfo(mediaPart).suffix().toLower();
                if (extension.isEmpty())
                    extension = QStringLiteral("bin");
                const QString    fileName   = QStringLiteral("%1.%2").arg(imageIndex, 3, 10, QLatin1Char('0')).arg(extension);
                const QString    outputPath = QDir(styleDir).absoluteFilePath(fileName);
                const QByteArray imageData  = source.readAll();
                QSaveFile        output(outputPath);
                if (!output.open(QIODevice::WriteOnly) || output.write(imageData) != imageData.size() || !output.commit())
                {
                    warnings << QStringLiteral("无法保存款式图片: %1").arg(outputPath);
                    continue;
                }
                extracted.imagePaths.push_back(outputPath);
                ++imageIndex;
            }

            if (!extracted.imagePaths.isEmpty())
                styles.push_back(std::move(extracted));
        }
        return true;
    }

} // namespace

PptStyleExtractor::Result PptStyleExtractor::extract(const Options &options)
{
    Result result;
    if (options.pptxPath.isEmpty() || options.outputDir.isEmpty())
    {
        result.warnings << QStringLiteral("PPTX 路径或款式缓存目录为空");
        return result;
    }

    const QString openXmlDir =
        options.openXmlDir.isEmpty() ? QFileInfo(options.outputDir).dir().absoluteFilePath(QStringLiteral("openxml")) : options.openXmlDir;
    const int totalPages = options.pages.size();
    if (options.progress)
        options.progress(0, totalPages, QStringLiteral("正在解压 PPTX..."));
    if (!unpackPptx(options.pptxPath, openXmlDir, result.warnings))
        return result;

    if (options.progress)
        options.progress(0, totalPages, QStringLiteral("PPTX 解压完成，正在读取页面结构..."));
    const PresentationInfo presentation = readPresentation(openXmlDir, result.warnings);
    QVector<PendingStyle>  pendingStyles;
    QHash<QString, int>    styleIndexes;
    bool                   parsedAnySlide = false;
    QSet<int>              visitedPages;

    int pagePosition = 0;
    for (int page : options.pages)
    {
        ++pagePosition;
        if (options.progress)
        {
            options.progress(pagePosition - 1, totalPages, QStringLiteral("正在解析 PPT 第 %1 页...").arg(page));
        }
        if (page <= 0 || visitedPages.contains(page))
            continue;
        visitedPages.insert(page);

        QString slidePart;
        if (page <= presentation.slideParts.size())
            slidePart = presentation.slideParts.at(page - 1);
        if (slidePart.isEmpty())
        {
            slidePart = QStringLiteral("ppt/slides/slide%1.xml").arg(page);
            result.warnings << QStringLiteral("第 %1 页未在 presentation.xml 中找到，尝试 %2").arg(page).arg(slidePart);
        }

        const QByteArray slideXml = readPart(openXmlDir, slidePart, result.warnings);
        if (slideXml.isEmpty())
            continue;
        parsedAnySlide = true;

        const SlideContents          contents           = readSlide(slideXml, result.warnings, page);
        const QMap<QString, QString> imageRelationships = readRelationships(openXmlDir, slidePart, QStringLiteral("/image"), result.warnings);
        int                          unassignedPictures = 0;
        const auto                   cells = assignPicturesToLabels(contents, presentation.width, presentation.height, unassignedPictures);

        int matchedImages = 0;
        for (const auto &cell : cells)
        {
            int styleIndex = styleIndexes.value(cell.first, -1);
            if (styleIndex < 0)
            {
                styleIndex = pendingStyles.size();
                styleIndexes.insert(cell.first, styleIndex);
                pendingStyles.push_back({cell.first, page, {}});
            }

            PendingStyle &pending = pendingStyles[styleIndex];
            for (const QString &id : cell.second)
            {
                const QString mediaPart = imageRelationships.value(id);
                if (mediaPart.isEmpty())
                {
                    result.warnings << QStringLiteral("第 %1 页款号 %2 的图片关系无法解析: %3").arg(page).arg(cell.first, id);
                    continue;
                }
                if (!pending.mediaParts.contains(mediaPart))
                {
                    pending.mediaParts.push_back(mediaPart);
                    ++matchedImages;
                }
            }
        }

        result.warnings << QStringLiteral("第 %1 页识别到 %2 个款式、%3 张手绘图").arg(page).arg(cells.size()).arg(matchedImages);
        if (unassignedPictures > 0)
        {
            result.warnings << QStringLiteral("第 %1 页有 %2 张图片不在款号下方格子内，已忽略").arg(page).arg(unassignedPictures);
        }
        if (options.progress)
        {
            options.progress(pagePosition,
                             totalPages,
                             QStringLiteral("PPT 第 %1 页解析完成：%2 个款式、%3 张手绘图").arg(page).arg(cells.size()).arg(matchedImages));
        }
    }

    if (!parsedAnySlide)
        return result;
    if (options.progress)
        options.progress(totalPages, totalPages, QStringLiteral("正在按款号保存手绘图..."));
    writeExtractedStyles(openXmlDir, options.outputDir, pendingStyles, result.styles, result.warnings);
    if (options.progress)
        options.progress(totalPages, totalPages, QStringLiteral("手绘图保存完成，正在更新款号小图库..."));
    return result;
}
