#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>
#include <QVariantMap>

#include "ImageMetadata.h"

namespace
{

    bool check(bool condition, const QString &message)
    {
        if (!condition)
        {
            qCritical().noquote() << message;
        }
        return condition;
    }

    // These literals are byte offsets, marker values, and field widths from the JPEG/EXIF/IPTC fixture format.
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    void appendBe16(QByteArray &data, quint16 value)
    {
        data.append(static_cast<char>(value >> 8));
        data.append(static_cast<char>(value));
    }

    void appendBe32(QByteArray &data, quint32 value)
    {
        data.append(static_cast<char>(value >> 24));
        data.append(static_cast<char>(value >> 16));
        data.append(static_cast<char>(value >> 8));
        data.append(static_cast<char>(value));
    }

    void appendLe16(QByteArray &data, quint16 value)
    {
        data.append(static_cast<char>(value));
        data.append(static_cast<char>(value >> 8));
    }

    void appendLe32(QByteArray &data, quint32 value)
    {
        data.append(static_cast<char>(value));
        data.append(static_cast<char>(value >> 8));
        data.append(static_cast<char>(value >> 16));
        data.append(static_cast<char>(value >> 24));
    }

    void appendSegment(QByteArray &jpeg, uchar marker, const QByteArray &payload)
    {
        jpeg.append(static_cast<char>(0xff));
        jpeg.append(static_cast<char>(marker));
        appendBe16(jpeg, static_cast<quint16>(payload.size() + 2));
        jpeg.append(payload);
    }

    QByteArray makeExif()
    {
        QByteArray tiff("II", 2);
        appendLe16(tiff, 42);
        appendLe32(tiff, 8);

        appendLe16(tiff, 2);
        appendLe16(tiff, 0x0132);
        appendLe16(tiff, 2);
        appendLe32(tiff, 20);
        appendLe32(tiff, 38);
        appendLe16(tiff, 0x8769);
        appendLe16(tiff, 4);
        appendLe32(tiff, 1);
        appendLe32(tiff, 58);
        appendLe32(tiff, 0);
        tiff.append("2026:06:13 15:20:19", 20);

        appendLe16(tiff, 2);
        appendLe16(tiff, 0x829a);
        appendLe16(tiff, 5);
        appendLe32(tiff, 1);
        appendLe32(tiff, 88);
        appendLe16(tiff, 0x9003);
        appendLe16(tiff, 2);
        appendLe32(tiff, 20);
        appendLe32(tiff, 96);
        appendLe32(tiff, 0);
        appendLe32(tiff, 1);
        appendLe32(tiff, 125);
        tiff.append("2026:06:10 16:31:10", 20);

        return QByteArray("Exif\0\0", 6) + tiff;
    }

    QByteArray makeIptc()
    {
        QByteArray iim;
        iim.append(static_cast<char>(0x1c));
        iim.append(static_cast<char>(2));
        iim.append(static_cast<char>(55));
        appendBe16(iim, 8);
        iim.append("20260613", 8);

        QByteArray payload("Photoshop 3.0\0", 14);
        payload.append("8BIM", 4);
        appendBe16(payload, 0x0404);
        payload.append('\0');
        payload.append('\0');
        appendBe32(payload, static_cast<quint32>(iim.size()));
        payload.append(iim);
        payload.append('\0');
        return payload;
    }

    QByteArray makeJpeg()
    {
        QByteArray jpeg("\xff\xd8", 2);
        QByteArray jfif("JFIF\0", 5);
        jfif.append('\x01');
        jfif.append('\x02');
        jfif.append('\x01');
        appendBe16(jfif, 300);
        appendBe16(jfif, 300);
        jfif.append('\0');
        jfif.append('\0');
        appendSegment(jpeg, 0xe0, jfif);
        appendSegment(jpeg, 0xe1, makeExif());
        appendSegment(jpeg, 0xed, makeIptc());

        QByteArray sof;
        sof.append('\x08');
        appendBe16(sof, 240);
        appendBe16(sof, 320);
        sof.append('\x03');
        for (int component = 1; component <= 3; ++component)
        {
            sof.append(static_cast<char>(component));
            sof.append('\x11');
            sof.append('\0');
        }
        appendSegment(jpeg, 0xc0, sof);
        jpeg.append("\xff\xd9", 2);
        return jpeg;
    }
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    QString valueFor(const QVariantList &entries, const QString &name)
    {
        for (const QVariant &entry : entries)
        {
            const QVariantMap fields = entry.toMap();
            if (fields.value(QStringLiteral("name")).toString() == name)
            {
                return fields.value(QStringLiteral("value")).toString();
            }
        }
        return {};
    }

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir    temporary;
    if (!check(temporary.isValid(), QStringLiteral("无法创建图片元数据测试目录")))
    {
        return 1;
    }

    const QString path = temporary.filePath(QStringLiteral("sample.jpg"));
    QFile         file(path);
    if (!check(file.open(QIODevice::WriteOnly) && file.write(makeJpeg()) > 0, QStringLiteral("无法写入 JPEG 元数据测试文件")))
    {
        return 1;
    }
    file.close();

    ImageMetadata metadata;
    metadata.setImagePath(path);
    if (!check(valueFor(metadata.fileInfo(), QStringLiteral("文件名")) == QStringLiteral("sample.jpg"),
               QStringLiteral("文件信息必须显示当前图片文件名")))
    {
        return 1;
    }
    if (!check(valueFor(metadata.imageInfo(), QStringLiteral("宽度")) == QStringLiteral("320 像素") &&
                   valueFor(metadata.imageInfo(), QStringLiteral("高度")) == QStringLiteral("240 像素"),
               QStringLiteral("图像信息必须读取 JPEG SOF 尺寸")))
    {
        return 1;
    }
    if (!check(valueFor(metadata.imageInfo(), QStringLiteral("像素密度")) == QStringLiteral("300 × 300 DPI"),
               QStringLiteral("图像信息必须读取 JFIF 像素密度")))
    {
        return 1;
    }
    if (!check(valueFor(metadata.iptc(), QStringLiteral("创建日期")) == QStringLiteral("20260613"),
               QStringLiteral("IPTC 页必须读取 Photoshop IPTC 创建日期")))
    {
        return 1;
    }
    if (!check(valueFor(metadata.exif(), QStringLiteral("修改时间")) == QStringLiteral("2026:06:13 15:20:19") &&
                   valueFor(metadata.exif(), QStringLiteral("曝光时间 [秒]")) == QStringLiteral("1/125") &&
                   valueFor(metadata.exif(), QStringLiteral("拍摄时间")) == QStringLiteral("2026:06:10 16:31:10"),
               QStringLiteral("EXIF 页必须读取主 IFD 与 EXIF 子 IFD")))
    {
        return 1;
    }

    metadata.setImagePath(temporary.filePath(QStringLiteral("missing.jpg")));
    if (!check(metadata.fileInfo().isEmpty() && metadata.imageInfo().isEmpty() && metadata.iptc().isEmpty() && metadata.exif().isEmpty(),
               QStringLiteral("图片不存在时必须清空上一张图片的属性")))
    {
        return 1;
    }
    return 0;
}
