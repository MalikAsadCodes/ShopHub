#include "common/QrCode.h"

#include <qrcodegen.hpp>

#include <QtDebug>

#include <exception>

namespace shophub {

QrCode QrCode::encode(const QString &text)
{
    QrCode out;
    const QByteArray utf8 = text.toUtf8();
    try {
        const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(utf8.constData(), qrcodegen::QrCode::Ecc::MEDIUM);
        out.m_size = qr.getSize();
        out.m_modules.resize(out.m_size * out.m_size);
        for (int y = 0; y < out.m_size; ++y) {
            for (int x = 0; x < out.m_size; ++x)
                out.m_modules[y * out.m_size + x] = qr.getModule(x, y);
        }
    } catch (const std::exception &e) {
        qWarning() << "QR code could not be made:" << e.what();
        return QrCode();
    }
    return out;
}

bool QrCode::isDark(int x, int y) const
{
    if (x < 0 || y < 0 || x >= m_size || y >= m_size)
        return false;
    return m_modules[y * m_size + x];
}

} // namespace shophub
