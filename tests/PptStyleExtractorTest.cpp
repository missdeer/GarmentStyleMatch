#include <archive.h>
#include <archive_entry.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "PptStyleExtractor.h"

namespace
{
    constexpr int kArchiveFilePermissions = 0644;

    bool addZipEntry(struct archive *writer, const QByteArray &name, const QByteArray &data)
    {
        struct archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, name.constData());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, kArchiveFilePermissions);
        archive_entry_set_size(entry, data.size());
        const bool entryWritten =
            archive_write_header(writer, entry) == ARCHIVE_OK && archive_write_data(writer, data.constData(), data.size()) == data.size();
        archive_entry_free(entry);
        return entryWritten;
    }

    bool createFixture(const QString &path)
    {
        struct archive *writer = archive_write_new();
        archive_write_set_format_zip(writer);
#ifdef Q_OS_WIN
        const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
        const int          openResult = archive_write_open_filename_w(writer, nativePath.c_str());
#else
        const QByteArray nativePath = QFile::encodeName(path);
        const int        openResult = archive_write_open_filename(writer, nativePath.constData());
#endif
        if (openResult != ARCHIVE_OK)
        {
            archive_write_free(writer);
            return false;
        }

        const QByteArray presentation     = R"xml(
<p:presentation xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"
 xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
 <p:sldIdLst><p:sldId id="256" r:id="rId5"/><p:sldId id="257" r:id="rId6"/></p:sldIdLst>
 <p:sldSz cx="1200" cy="1000"/>
</p:presentation>)xml";
        const QByteArray presentationRels = R"xml(
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
 <Relationship Id="rId5" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide2.xml"/>
 <Relationship Id="rId6" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide1.xml"/>
</Relationships>)xml";
        const QByteArray slide            = R"xml(
<p:sld xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"
 xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"
 xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
 <p:cSld><p:spTree>
  <p:nvGrpSpPr/><p:grpSpPr/>
  <p:graphicFrame>
   <p:nvGraphicFramePr/><p:xfrm><a:off x="0" y="0"/><a:ext cx="1200" cy="800"/></p:xfrm>
   <a:graphic><a:graphicData><a:tbl>
    <a:tblGrid><a:gridCol w="600"/><a:gridCol w="600"/></a:tblGrid>
    <a:tr h="100"><a:tc><a:txBody><a:p><a:r><a:t>STYLE100A</a:t></a:r></a:p></a:txBody></a:tc><a:tc><a:txBody><a:p><a:r><a:t>STYLE200B</a:t></a:r></a:p></a:txBody></a:tc></a:tr>
    <a:tr h="300"><a:tc/><a:tc/></a:tr>
    <a:tr h="100"><a:tc><a:txBody><a:p><a:r><a:t>STYLE300C</a:t></a:r></a:p></a:txBody></a:tc><a:tc/></a:tr>
    <a:tr h="300"><a:tc/><a:tc/></a:tr>
   </a:tbl></a:graphicData></a:graphic>
  </p:graphicFrame>
  <p:pic><p:nvPicPr/><p:blipFill><a:blip r:embed="rId1"/></p:blipFill><p:spPr><a:xfrm><a:off x="50" y="120"/><a:ext cx="220" cy="240"/></a:xfrm></p:spPr></p:pic>
  <p:pic><p:nvPicPr/><p:blipFill><a:blip r:embed="rId2"/></p:blipFill><p:spPr><a:xfrm><a:off x="300" y="140"/><a:ext cx="220" cy="220"/></a:xfrm></p:spPr></p:pic>
  <p:pic><p:nvPicPr/><p:blipFill><a:blip r:embed="rId3"/></p:blipFill><p:spPr><a:xfrm><a:off x="700" y="120"/><a:ext cx="300" cy="240"/></a:xfrm></p:spPr></p:pic>
  <p:pic><p:nvPicPr/><p:blipFill><a:blip r:embed="rId4"/></p:blipFill><p:spPr><a:xfrm><a:off x="100" y="520"/><a:ext cx="350" cy="240"/></a:xfrm></p:spPr></p:pic>
  <p:pic><p:nvPicPr/><p:blipFill><a:blip r:embed="rIdBg"/></p:blipFill><p:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="1200" cy="1000"/></a:xfrm></p:spPr></p:pic>
 </p:spTree></p:cSld>
</p:sld>)xml";
        const QByteArray slideRels        = R"xml(
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
 <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="../media/a.png"/>
 <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="../media/b.png"/>
 <Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="../media/c.png"/>
 <Relationship Id="rId4" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="../media/d.png"/>
 <Relationship Id="rIdBg" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="../media/background.png"/>
</Relationships>)xml";
        const QByteArray otherSlide =
            R"xml(<p:sld xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"><p:cSld><p:spTree/></p:cSld></p:sld>)xml";

        const bool archiveWritten =
            addZipEntry(writer, "ppt/presentation.xml", presentation) && addZipEntry(writer, "ppt/_rels/presentation.xml.rels", presentationRels) &&
            addZipEntry(writer, "ppt/slides/slide2.xml", slide) && addZipEntry(writer, "ppt/slides/_rels/slide2.xml.rels", slideRels) &&
            addZipEntry(writer, "ppt/slides/slide1.xml", otherSlide) && addZipEntry(writer, "ppt/media/a.png", "image-a") &&
            addZipEntry(writer, "ppt/media/b.png", "image-b") && addZipEntry(writer, "ppt/media/c.png", "image-c") &&
            addZipEntry(writer, "ppt/media/d.png", "image-d") && addZipEntry(writer, "ppt/media/background.png", "background");
        const bool closed = archive_write_close(writer) == ARCHIVE_OK;
        archive_write_free(writer);
        return archiveWritten && closed;
    }

    bool check(bool condition, const QString &message)
    {
        if (!condition)
        {
            qCritical().noquote() << message;
        }
        return condition;
    }

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir    temporary;
    if (!check(temporary.isValid(), QStringLiteral("无法创建测试临时目录")))
    {
        return 1;
    }

    const QString pptxPath = temporary.filePath(QStringLiteral("fixture.pptx"));
    if (!check(createFixture(pptxPath), QStringLiteral("无法创建 PPTX 测试包")))
    {
        return 1;
    }

    const QString outputDir = temporary.filePath(QStringLiteral("styles"));
    QDir().mkpath(outputDir);
    QFile stale(QDir(outputDir).filePath(QStringLiteral("stale.txt")));
    if (!check(stale.open(QIODevice::WriteOnly) && stale.write("stale") > 0, QStringLiteral("无法创建待清理的旧缓存文件")))
    {
        return 1;
    }
    stale.close();

    PptStyleExtractor::Options options;
    options.pptxPath   = pptxPath;
    options.pages      = {1};
    options.outputDir  = outputDir;
    options.openXmlDir = temporary.filePath(QStringLiteral("openxml"));
    QVector<int> progressValues;
    QStringList  progressMessages;
    options.progress = [&progressValues, &progressMessages](int current, int, const QString &message) {
        progressValues.push_back(current);
        progressMessages.push_back(message);
    };
    const PptStyleExtractor::Result result = PptStyleExtractor::extract(options);

    if (!check(result.styles.size() == 3,
               QStringLiteral("应从放映顺序第 1 页提取 3 个款式，实际为 %1；日志：%2")
                   .arg(result.styles.size())
                   .arg(result.warnings.join(QStringLiteral(" | ")))))
    {
        return 1;
    }
    if (!check(result.styles.at(0).styleId == QStringLiteral("STYLE100A") && result.styles.at(0).imagePaths.size() == 2,
               QStringLiteral("STYLE100A 应对应两张图片")))
    {
        return 1;
    }
    if (!check(result.styles.at(1).styleId == QStringLiteral("STYLE200B") && result.styles.at(1).imagePaths.size() == 1,
               QStringLiteral("STYLE200B 应对应一张图片")))
    {
        return 1;
    }
    if (!check(result.styles.at(2).styleId == QStringLiteral("STYLE300C") && result.styles.at(2).imagePaths.size() == 1,
               QStringLiteral("STYLE300C 应对应下一行的一张图片")))
    {
        return 1;
    }
    if (!check(!QFileInfo::exists(QDir(outputDir).filePath(QStringLiteral("stale.txt"))), QStringLiteral("重新提取前应清理旧款式缓存")))
    {
        return 1;
    }
    if (!check(QFileInfo::exists(QDir(options.openXmlDir).filePath(QStringLiteral("ppt/presentation.xml"))),
               QStringLiteral("PPTX 应先解压到 Open XML 缓存")))
    {
        return 1;
    }
    if (!check(!progressValues.isEmpty() && progressValues.front() == 0 && progressValues.back() == 1,
               QStringLiteral("提取进度必须从 0 开始并到达选中页总数")))
    {
        return 1;
    }
    if (!check(progressMessages.join(QLatin1Char('|')).contains(QStringLiteral("正在解析 PPT 第 1 页")),
               QStringLiteral("进度消息必须包含当前解析页码")))
    {
        return 1;
    }

    return 0;
}
