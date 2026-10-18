#include "services/ImageStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QtDebug>

namespace shophub {

namespace {

// Detects the image type from its first bytes rather than trusting the extension.
QString detectExtension(const QByteArray &h)
{
    if (h.startsWith("\x89PNG\r\n\x1a\n"))
        return QStringLiteral(".png");
    if (h.startsWith("\xFF\xD8\xFF"))
        return QStringLiteral(".jpg");
    if (h.size() >= 12 && h.startsWith("RIFF") && h.mid(8, 4) == "WEBP")
        return QStringLiteral(".webp");
    if (h.startsWith("GIF87a") || h.startsWith("GIF89a"))
        return QStringLiteral(".gif");
    if (h.startsWith("BM"))
        return QStringLiteral(".bmp");
    return QString();
}

} // namespace

ImageStore::ImageStore(QString directory)
    : m_directory(std::move(directory))
{
    QDir().mkpath(m_directory);
}

Result<QString> ImageStore::import(const QString &sourcePath) const
{
    const QFileInfo info(sourcePath);
    if (!info.exists() || !info.isFile())
        return Result<QString>::fail(QStringLiteral("The selected image file does not exist."));
    if (info.size() > kMaxBytes)
        return Result<QString>::fail(QStringLiteral("Images must be 5 MB or smaller."));

    QFile in(sourcePath);
    if (!in.open(QIODevice::ReadOnly))
        return Result<QString>::fail(QStringLiteral("The selected image could not be read."));
    const QString ext = detectExtension(in.peek(16));
    in.close();
    if (ext.isEmpty())
        return Result<QString>::fail(QStringLiteral("Please choose a PNG, JPEG, WebP, GIF or BMP image."));

    const QString name = QUuid::createUuid().toString(QUuid::WithoutBraces) + ext;
    const QString target = QDir(m_directory).filePath(name);
    if (!QFile::copy(sourcePath, target)) {
        qWarning().noquote() << "Image copy failed into" << m_directory;
        return Result<QString>::fail(QStringLiteral("The image could not be saved."));
    }
    qInfo().noquote() << "Imported product image" << name;
    return name;
}

QString ImageStore::resolve(const QString &fileName) const
{
    if (fileName.isEmpty() || fileName.contains(QLatin1Char('/')) || fileName.contains(QLatin1Char('\\'))
        || fileName.contains(QLatin1String("..")))
        return QString();
    const QString path = QDir(m_directory).filePath(fileName);
    return QFileInfo::exists(path) ? path : QString();
}

} // namespace shophub
