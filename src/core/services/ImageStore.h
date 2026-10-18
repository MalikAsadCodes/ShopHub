#pragma once

#include "common/Result.h"

#include <QString>

namespace shophub {

// Product images are copied into the app's own data folder under a random
// file name, so the database only stores that name (never a user path) and
// backing up the data folder backs up the images too.
class ImageStore
{
public:
    static constexpr qint64 kMaxBytes = 5 * 1024 * 1024;

    explicit ImageStore(QString directory);

    // Validates (PNG/JPEG/WebP/GIF/BMP, max 5 MB) and copies the file.
    // Returns the stored file name.
    Result<QString> import(const QString &sourcePath) const;

    // Absolute path for a stored file name, or empty if invalid/missing.
    QString resolve(const QString &fileName) const;

    const QString &directory() const { return m_directory; }

private:
    QString m_directory;
};

} // namespace shophub
