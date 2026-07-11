#include <array>
#include <cmath>
#include <optional>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QLocale>
#include <QMimeDatabase>
#include <QVariantMap>

#include "ImageMetadata.h"

// Binary metadata parsers intentionally use format-defined offsets, marker values, and byte-level access.
// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,cppcoreguidelines-avoid-do-while,cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-bounds-array-to-pointer-decay,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-reinterpret-cast,modernize-avoid-c-arrays,readability-function-cognitive-complexity,readability-identifier-length,readability-magic-numbers)
namespace
{

    struct JpegMetadata
    {
        int          width      = 0;
        int          height     = 0;
        int          precision  = 0;
        int          components = 0;
        double       xDpi       = 0.0;
        double       yDpi       = 0.0;
        QString      comment;
        QVariantList iptc;
        QVariantList exif;
    };

    void appendEntry(QVariantList &entries, const QString &name, const QString &value)
    {
        if (value.isEmpty())
        {
            return;
        }
        entries.append(QVariantMap {{QStringLiteral("name"), name}, {QStringLiteral("value"), value}});
    }

    quint16 readBe16(const QByteArray &data, qsizetype offset)
    {
        if (offset < 0 || offset + 2 > data.size())
        {
            return 0;
        }
        const auto *p = reinterpret_cast<const uchar *>(data.constData() + offset);
        return static_cast<quint16>((p[0] << 8) | p[1]);
    }

    quint32 readBe32(const QByteArray &data, qsizetype offset)
    {
        if (offset < 0 || offset + 4 > data.size())
        {
            return 0;
        }
        const auto *p = reinterpret_cast<const uchar *>(data.constData() + offset);
        return (static_cast<quint32>(p[0]) << 24) | (static_cast<quint32>(p[1]) << 16) | (static_cast<quint32>(p[2]) << 8) |
               static_cast<quint32>(p[3]);
    }

    QString decodeText(const QByteArray &bytes)
    {
        const QString utf8 = QString::fromUtf8(bytes);
        return utf8.contains(QChar::ReplacementCharacter) ? QString::fromLatin1(bytes).trimmed() : utf8.trimmed();
    }

    QString iptcName(int record, int dataset)
    {
        if (record == 1 && dataset == 90)
        {
            return QStringLiteral("字符集");
        }
        if (record != 2)
        {
            return QStringLiteral("%1:%2").arg(record).arg(dataset);
        }

        switch (dataset)
        {
        case 5:
            return QStringLiteral("对象名称");
        case 25:
            return QStringLiteral("关键词");
        case 55:
            return QStringLiteral("创建日期");
        case 60:
            return QStringLiteral("创建时间");
        case 62:
            return QStringLiteral("数字化日期");
        case 63:
            return QStringLiteral("数字化时间");
        case 80:
            return QStringLiteral("作者");
        case 90:
            return QStringLiteral("城市");
        case 95:
            return QStringLiteral("省/州");
        case 101:
            return QStringLiteral("国家/地区");
        case 105:
            return QStringLiteral("标题");
        case 110:
            return QStringLiteral("来源");
        case 116:
            return QStringLiteral("版权声明");
        case 120:
            return QStringLiteral("说明");
        default:
            return QStringLiteral("IPTC 2:%1").arg(dataset);
        }
    }

    QVariantList parseIptcBlock(const QByteArray &data)
    {
        QVariantList result;
        qsizetype    offset = 0;
        while (offset + 5 <= data.size())
        {
            if (static_cast<uchar>(data.at(offset)) != 0x1c)
            {
                ++offset;
                continue;
            }
            const int     record  = static_cast<uchar>(data.at(offset + 1));
            const int     dataset = static_cast<uchar>(data.at(offset + 2));
            const quint16 length  = readBe16(data, offset + 3);
            offset += 5;
            if (offset + length > data.size())
            {
                break;
            }
            const QByteArray value = data.mid(offset, length);
            offset += length;
            appendEntry(result, iptcName(record, dataset), decodeText(value));
        }
        return result;
    }

    QVariantList parsePhotoshopIptc(const QByteArray &payload)
    {
        static constexpr char kHeader[] = "Photoshop 3.0\0";
        if (!payload.startsWith(QByteArray(kHeader, sizeof(kHeader) - 1)))
        {
            return {};
        }

        qsizetype offset = sizeof(kHeader) - 1;
        while (offset + 12 <= payload.size())
        {
            if (payload.mid(offset, 4) != QByteArrayLiteral("8BIM"))
            {
                break;
            }
            const quint16 resourceId = readBe16(payload, offset + 4);
            offset += 6;

            const int       nameLength      = static_cast<uchar>(payload.at(offset));
            const qsizetype nameFieldLength = 1 + nameLength;
            offset += nameFieldLength + (nameFieldLength % 2);
            if (offset + 4 > payload.size())
            {
                break;
            }

            const quint32 dataLength = readBe32(payload, offset);
            offset += 4;
            if (dataLength > static_cast<quint32>(payload.size() - offset))
            {
                break;
            }
            if (resourceId == 0x0404)
            {
                return parseIptcBlock(payload.mid(offset, dataLength));
            }
            offset += dataLength + (dataLength % 2);
        }
        return {};
    }

    class TiffReader
    {
    public:
        explicit TiffReader(QByteArray data) : m_data(std::move(data))
        {
            if (m_data.size() >= 8)
            {
                m_littleEndian = m_data.startsWith("II");
                m_valid        = (m_littleEndian || m_data.startsWith("MM")) && u16(2) == 42;
            }
        }

        [[nodiscard]] bool isValid() const
        {
            return m_valid;
        }

        [[nodiscard]] quint16 u16(quint32 offset) const
        {
            if (offset + 2 > static_cast<quint32>(m_data.size()))
            {
                return 0;
            }
            const auto *p = reinterpret_cast<const uchar *>(m_data.constData() + offset);
            return m_littleEndian ? static_cast<quint16>(p[0] | (p[1] << 8)) : static_cast<quint16>((p[0] << 8) | p[1]);
        }

        [[nodiscard]] quint32 u32(quint32 offset) const
        {
            if (offset + 4 > static_cast<quint32>(m_data.size()))
            {
                return 0;
            }
            const auto *p = reinterpret_cast<const uchar *>(m_data.constData() + offset);
            if (m_littleEndian)
            {
                return static_cast<quint32>(p[0]) | (static_cast<quint32>(p[1]) << 8) | (static_cast<quint32>(p[2]) << 16) |
                       (static_cast<quint32>(p[3]) << 24);
            }
            return (static_cast<quint32>(p[0]) << 24) | (static_cast<quint32>(p[1]) << 16) | (static_cast<quint32>(p[2]) << 8) |
                   static_cast<quint32>(p[3]);
        }

        [[nodiscard]] qint32 s32(quint32 offset) const
        {
            return static_cast<qint32>(u32(offset));
        }

        [[nodiscard]] QByteArray bytes(quint32 offset, quint32 length) const
        {
            if (offset > static_cast<quint32>(m_data.size()) || length > static_cast<quint32>(m_data.size()) - offset)
            {
                return {};
            }
            return m_data.mid(offset, length);
        }

    private:
        QByteArray m_data;
        bool       m_littleEndian = false;
        bool       m_valid        = false;
    };

    quint32 tiffTypeSize(quint16 type)
    {
        switch (type)
        {
        case 1:
        case 2:
        case 6:
        case 7:
            return 1;
        case 3:
        case 8:
            return 2;
        case 4:
        case 9:
        case 11:
            return 4;
        case 5:
        case 10:
        case 12:
            return 8;
        default:
            return 0;
        }
    }

    QString exifTagName(quint16 tag)
    {
        switch (tag)
        {
        case 0x010e:
            return QStringLiteral("图像说明");
        case 0x010f:
            return QStringLiteral("相机制造商");
        case 0x0110:
            return QStringLiteral("相机型号");
        case 0x0112:
            return QStringLiteral("方向");
        case 0x011a:
            return QStringLiteral("水平分辨率");
        case 0x011b:
            return QStringLiteral("垂直分辨率");
        case 0x0128:
            return QStringLiteral("分辨率单位");
        case 0x0131:
            return QStringLiteral("软件");
        case 0x0132:
            return QStringLiteral("修改时间");
        case 0x013b:
            return QStringLiteral("作者");
        case 0x8298:
            return QStringLiteral("版权");
        case 0x829a:
            return QStringLiteral("曝光时间 [秒]");
        case 0x829d:
            return QStringLiteral("F 值");
        case 0x8822:
            return QStringLiteral("曝光程序");
        case 0x8827:
            return QStringLiteral("ISO 感光度");
        case 0x9000:
            return QStringLiteral("EXIF 版本");
        case 0x9003:
            return QStringLiteral("拍摄时间");
        case 0x9004:
            return QStringLiteral("数字化时间");
        case 0x9201:
            return QStringLiteral("快门速度 [秒]");
        case 0x9202:
            return QStringLiteral("光圈");
        case 0x9204:
            return QStringLiteral("曝光补偿");
        case 0x9206:
            return QStringLiteral("主体距离 [米]");
        case 0x9207:
            return QStringLiteral("测光模式");
        case 0x9209:
            return QStringLiteral("闪光灯");
        case 0x920a:
            return QStringLiteral("焦距 [毫米]");
        case 0xa002:
            return QStringLiteral("像素宽度");
        case 0xa003:
            return QStringLiteral("像素高度");
        case 0xa403:
            return QStringLiteral("白平衡");
        case 0xa405:
            return QStringLiteral("35mm 等效焦距");
        default:
            return {};
        }
    }

    QString rationalText(qint64 numerator, qint64 denominator)
    {
        if (denominator == 0)
        {
            return {};
        }
        if (numerator > 0 && denominator > numerator && denominator % numerator == 0)
        {
            return QStringLiteral("1/%1").arg(denominator / numerator);
        }
        const double value = static_cast<double>(numerator) / static_cast<double>(denominator);
        return QLocale::c().toString(value, 'g', 6);
    }

    QString exifValue(const TiffReader &reader, quint16 type, quint32 count, quint32 valueFieldOffset, quint32 dataOffset)
    {
        if (count == 0)
        {
            return {};
        }
        switch (type)
        {
        case 1:
        case 7: {
            const QByteArray value = reader.bytes(dataOffset, count);
            if (type == 7 && count == 4)
            {
                return QString::fromLatin1(value);
            }
            QStringList values;
            values.reserve(static_cast<qsizetype>(count));
            for (const char byte : value)
            {
                values.append(QString::number(static_cast<uchar>(byte)));
            }
            return values.join(QStringLiteral(", "));
        }
        case 2: {
            QByteArray      value = reader.bytes(dataOffset, count);
            const qsizetype nul   = value.indexOf('\0');
            if (nul >= 0)
            {
                value.truncate(nul);
            }
            return decodeText(value);
        }
        case 3: {
            QStringList values;
            for (quint32 i = 0; i < count; ++i)
            {
                values.append(QString::number(reader.u16(dataOffset + i * 2)));
            }
            return values.join(QStringLiteral(", "));
        }
        case 4: {
            QStringList values;
            for (quint32 i = 0; i < count; ++i)
            {
                values.append(QString::number(reader.u32(dataOffset + i * 4)));
            }
            return values.join(QStringLiteral(", "));
        }
        case 5:
            return rationalText(reader.u32(dataOffset), reader.u32(dataOffset + 4));
        case 9:
            return QString::number(reader.s32(dataOffset));
        case 10:
            return rationalText(reader.s32(dataOffset), reader.s32(dataOffset + 4));
        default:
            Q_UNUSED(valueFieldOffset)
            return {};
        }
    }

    QString describeExifValue(quint16 tag, const QString &value)
    {
        bool      ok     = false;
        const int number = value.toInt(&ok);
        if (tag == 0x0112 && ok)
        {
            static const std::array<const char *, 8> orientations = {
                "左上", "右上", "右下", "左下", "左上（镜像）", "右上（镜像）", "右下（镜像）", "左下（镜像）"};
            if (number >= 1 && number <= static_cast<int>(orientations.size()))
            {
                return QStringLiteral("%1 (%2)").arg(QString::fromUtf8(orientations[number - 1])).arg(number);
            }
        }
        if (tag == 0x0128 && ok)
        {
            if (number == 2)
            {
                return QStringLiteral("英寸");
            }
            if (number == 3)
            {
                return QStringLiteral("厘米");
            }
        }
        if (tag == 0x8822 && ok)
        {
            static const std::array<const char *, 9> programs = {
                "未定义", "手动", "标准程序", "光圈优先", "快门优先", "创意程序", "运动程序", "人像", "风景"};
            if (number >= 0 && number < static_cast<int>(programs.size()))
            {
                return QStringLiteral("%1 (%2)").arg(QString::fromUtf8(programs[number])).arg(number);
            }
        }
        if (tag == 0x9207 && ok)
        {
            static const std::array<const char *, 7> modes = {"未知", "平均", "中央重点平均", "点测光", "多点测光", "多区测光", "局部测光"};
            if (number >= 0 && number < static_cast<int>(modes.size()))
            {
                return QStringLiteral("%1 (%2)").arg(QString::fromUtf8(modes[number])).arg(number);
            }
        }
        if (tag == 0x9209 && ok)
        {
            return number & 1 ? QStringLiteral("闪光灯已触发") : QStringLiteral("未使用闪光灯");
        }
        if (tag == 0xa403 && ok)
        {
            return number == 0 ? QStringLiteral("自动") : QStringLiteral("手动");
        }
        if (tag == 0x9201 || tag == 0x9202)
        {
            const double apex = value.toDouble(&ok);
            if (ok && tag == 0x9201)
            {
                const double seconds = std::pow(2.0, -apex);
                if (seconds > 0.0 && seconds < 1.0)
                {
                    return QStringLiteral("1/%1").arg(qRound(1.0 / seconds));
                }
                return QLocale::c().toString(seconds, 'g', 6);
            }
            if (ok)
            {
                return QStringLiteral("F%1").arg(QLocale::c().toString(std::pow(2.0, apex / 2.0), 'g', 3));
            }
        }
        return value;
    }

    struct IfdResult
    {
        QVariantList          entries;
        quint32               exifOffset = 0;
        std::optional<double> xResolution;
        std::optional<double> yResolution;
        int                   resolutionUnit = 0;
    };

    std::optional<double> rationalValue(const TiffReader &reader, quint32 offset)
    {
        const quint32 denominator = reader.u32(offset + 4);
        if (denominator == 0)
        {
            return std::nullopt;
        }
        return static_cast<double>(reader.u32(offset)) / denominator;
    }

    IfdResult parseIfd(const TiffReader &reader, quint32 offset)
    {
        IfdResult     result;
        const quint16 count = reader.u16(offset);
        if (count > 1024)
        {
            return result;
        }

        for (quint32 i = 0; i < count; ++i)
        {
            const quint32 entryOffset = offset + 2 + i * 12;
            const quint16 tag         = reader.u16(entryOffset);
            const quint16 type        = reader.u16(entryOffset + 2);
            const quint32 valueCount  = reader.u32(entryOffset + 4);
            const quint32 unitSize    = tiffTypeSize(type);
            if (unitSize == 0 || valueCount > 4096)
            {
                continue;
            }
            const quint32 byteCount  = unitSize * valueCount;
            const quint32 dataOffset = byteCount <= 4 ? entryOffset + 8 : reader.u32(entryOffset + 8);

            if (tag == 0x8769)
            {
                result.exifOffset = reader.u32(dataOffset);
                continue;
            }
            if (tag == 0x011a && type == 5)
            {
                result.xResolution = rationalValue(reader, dataOffset);
            }
            else if (tag == 0x011b && type == 5)
            {
                result.yResolution = rationalValue(reader, dataOffset);
            }
            else if (tag == 0x0128)
            {
                result.resolutionUnit = reader.u16(dataOffset);
            }

            const QString name = exifTagName(tag);
            if (!name.isEmpty())
            {
                const QString value = exifValue(reader, type, valueCount, entryOffset + 8, dataOffset);
                appendEntry(result.entries, name, describeExifValue(tag, value));
            }
        }
        return result;
    }

    void parseExif(const QByteArray &payload, JpegMetadata &metadata)
    {
        if (!payload.startsWith(QByteArrayLiteral("Exif\0\0")))
        {
            return;
        }
        const TiffReader reader(payload.mid(6));
        if (!reader.isValid())
        {
            return;
        }

        IfdResult primary = parseIfd(reader, reader.u32(4));
        metadata.exif     = primary.entries;
        if (primary.exifOffset != 0)
        {
            const IfdResult exif = parseIfd(reader, primary.exifOffset);
            metadata.exif.append(exif.entries);
        }

        const double unitScale = primary.resolutionUnit == 3 ? 2.54 : 1.0;
        if (primary.xResolution)
        {
            metadata.xDpi = *primary.xResolution * unitScale;
        }
        if (primary.yResolution)
        {
            metadata.yDpi = *primary.yResolution * unitScale;
        }
    }

    bool isStartOfFrame(uchar marker)
    {
        switch (marker)
        {
        case 0xc0:
        case 0xc1:
        case 0xc2:
        case 0xc3:
        case 0xc5:
        case 0xc6:
        case 0xc7:
        case 0xc9:
        case 0xca:
        case 0xcb:
        case 0xcd:
        case 0xce:
        case 0xcf:
            return true;
        default:
            return false;
        }
    }

    JpegMetadata readJpegMetadata(const QString &path)
    {
        JpegMetadata result;
        QFile        file(path);
        if (!file.open(QIODevice::ReadOnly) || file.read(2) != QByteArrayLiteral("\xff\xd8"))
        {
            return result;
        }

        while (!file.atEnd())
        {
            char byte = 0;
            do
            {
                if (!file.getChar(&byte))
                {
                    return result;
                }
            } while (static_cast<uchar>(byte) != 0xff);
            do
            {
                if (!file.getChar(&byte))
                {
                    return result;
                }
            } while (static_cast<uchar>(byte) == 0xff);

            const auto marker = static_cast<uchar>(byte);
            if (marker == 0xd9 || marker == 0xda)
            {
                break;
            }
            if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7))
            {
                continue;
            }

            const QByteArray lengthBytes   = file.read(2);
            const quint16    segmentLength = readBe16(lengthBytes, 0);
            if (segmentLength < 2)
            {
                break;
            }
            const QByteArray payload = file.read(segmentLength - 2);
            if (payload.size() != segmentLength - 2)
            {
                break;
            }

            if (marker == 0xe0 && payload.startsWith(QByteArrayLiteral("JFIF\0")) && payload.size() >= 12)
            {
                const int    units = static_cast<uchar>(payload.at(7));
                const double scale = units == 2 ? 2.54 : 1.0;
                if (units == 1 || units == 2)
                {
                    result.xDpi = readBe16(payload, 8) * scale;
                    result.yDpi = readBe16(payload, 10) * scale;
                }
            }
            else if (marker == 0xe1)
            {
                parseExif(payload, result);
            }
            else if (marker == 0xed)
            {
                const QVariantList iptc = parsePhotoshopIptc(payload);
                if (!iptc.isEmpty())
                {
                    result.iptc = iptc;
                }
            }
            else if (marker == 0xfe)
            {
                result.comment = decodeText(payload);
            }
            else if (isStartOfFrame(marker) && payload.size() >= 6)
            {
                result.precision  = static_cast<uchar>(payload.at(0));
                result.height     = readBe16(payload, 1);
                result.width      = readBe16(payload, 3);
                result.components = static_cast<uchar>(payload.at(5));
            }
        }
        return result;
    }

    QString formatFileSize(qint64 bytes)
    {
        static constexpr std::array<const char *, 4> units = {"B", "KB", "MB", "GB"};
        auto                                         value = static_cast<double>(bytes);
        qsizetype                                    unit  = 0;
        while (value >= 1024.0 && unit + 1 < static_cast<qsizetype>(units.size()))
        {
            value /= 1024.0;
            ++unit;
        }
        const int precision = unit == 0 ? 0 : 2;
        return QStringLiteral("%1 %2 (%3 字节)")
            .arg(QLocale().toString(value, 'f', precision), QString::fromLatin1(units[unit]), QLocale().toString(bytes));
    }

    QString formatDateTime(const QDateTime &value)
    {
        return value.isValid() ? value.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss")) : QString();
    }

    QString colorModel(int components)
    {
        switch (components)
        {
        case 1:
            return QStringLiteral("灰度");
        case 3:
            return QStringLiteral("RGB / YCbCr");
        case 4:
            return QStringLiteral("CMYK / YCCK");
        default:
            return components > 0 ? QStringLiteral("%1 分量").arg(components) : QString();
        }
    }

} // namespace

ImageMetadata::ImageMetadata(QObject *parent) : QObject(parent) {}

void ImageMetadata::setImagePath(const QString &path)
{
    if (path == m_imagePath)
    {
        return;
    }
    m_imagePath = path;
    reload();
    emit imagePathChanged();
}

void ImageMetadata::reload()
{
    m_fileInfo.clear();
    m_imageInfo.clear();
    m_iptc.clear();
    m_exif.clear();

    const QFileInfo file(m_imagePath);
    if (m_imagePath.isEmpty() || !file.exists() || !file.isFile())
    {
        emit metadataChanged();
        return;
    }

    appendEntry(m_fileInfo, QStringLiteral("文件名"), file.fileName());
    appendEntry(m_fileInfo, QStringLiteral("目录"), QDir::toNativeSeparators(file.absolutePath()));
    appendEntry(m_fileInfo, QStringLiteral("大小"), formatFileSize(file.size()));
    appendEntry(m_fileInfo, QStringLiteral("类型"), QMimeDatabase().mimeTypeForFile(file).comment());
    appendEntry(m_fileInfo, QStringLiteral("创建时间"), formatDateTime(file.birthTime()));
    appendEntry(m_fileInfo, QStringLiteral("修改时间"), formatDateTime(file.lastModified()));
    appendEntry(m_fileInfo, QStringLiteral("访问时间"), formatDateTime(file.lastRead()));
    appendEntry(m_fileInfo, QStringLiteral("只读"), file.isWritable() ? QStringLiteral("否") : QStringLiteral("是"));
    appendEntry(m_fileInfo, QStringLiteral("隐藏"), file.isHidden() ? QStringLiteral("是") : QStringLiteral("否"));

    QImageReader       reader(m_imagePath);
    const QByteArray   format = reader.format();
    const QSize        size   = reader.size();
    const JpegMetadata jpeg   = format.toLower() == QByteArrayLiteral("jpeg") || format.toLower() == QByteArrayLiteral("jpg")
                                    ? readJpegMetadata(m_imagePath)
                                    : JpegMetadata {};
    const int          width  = jpeg.width > 0 ? jpeg.width : size.width();
    const int          height = jpeg.height > 0 ? jpeg.height : size.height();

    appendEntry(m_imageInfo, QStringLiteral("格式"), format.isEmpty() ? QStringLiteral("未知") : QString::fromLatin1(format).toUpper());
    if (width > 0)
    {
        appendEntry(m_imageInfo, QStringLiteral("宽度"), QStringLiteral("%1 像素").arg(width));
    }
    if (height > 0)
    {
        appendEntry(m_imageInfo, QStringLiteral("高度"), QStringLiteral("%1 像素").arg(height));
    }
    if (jpeg.xDpi > 0.0 && jpeg.yDpi > 0.0)
    {
        appendEntry(m_imageInfo,
                    QStringLiteral("像素密度"),
                    QStringLiteral("%1 × %2 DPI").arg(QLocale::c().toString(jpeg.xDpi, 'f', 0), QLocale::c().toString(jpeg.yDpi, 'f', 0)));
        if (width > 0 && height > 0)
        {
            appendEntry(m_imageInfo,
                        QStringLiteral("打印尺寸"),
                        QStringLiteral("%1 × %2 英寸 / %3 × %4 厘米")
                            .arg(QLocale::c().toString(width / jpeg.xDpi, 'f', 2),
                                 QLocale::c().toString(height / jpeg.yDpi, 'f', 2),
                                 QLocale::c().toString(width / jpeg.xDpi * 2.54, 'f', 2),
                                 QLocale::c().toString(height / jpeg.yDpi * 2.54, 'f', 2)));
        }
    }
    if (jpeg.precision > 0 && jpeg.components > 0)
    {
        appendEntry(m_imageInfo, QStringLiteral("比特深度"), QStringLiteral("%1 位").arg(jpeg.precision * jpeg.components));
    }
    appendEntry(m_imageInfo, QStringLiteral("色彩模式"), colorModel(jpeg.components));
    appendEntry(m_imageInfo,
                QStringLiteral("压缩"),
                format.toLower() == QByteArrayLiteral("jpeg") ? QStringLiteral("JPEG") : QString::fromLatin1(format).toUpper());
    appendEntry(m_imageInfo, QStringLiteral("页/帧"), QString::number(qMax(1, reader.imageCount())));
    appendEntry(m_imageInfo, QStringLiteral("注释"), jpeg.comment);

    m_iptc = jpeg.iptc;
    m_exif = jpeg.exif;
    emit metadataChanged();
}
// NOLINTEND(cppcoreguidelines-avoid-c-arrays,cppcoreguidelines-avoid-do-while,cppcoreguidelines-avoid-magic-numbers,cppcoreguidelines-pro-bounds-array-to-pointer-decay,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-reinterpret-cast,modernize-avoid-c-arrays,readability-function-cognitive-complexity,readability-identifier-length,readability-magic-numbers)
