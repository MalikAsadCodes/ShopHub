#pragma once

#include <QString>
#include <QVector>

namespace shophub {

// A QR code as a square grid of dark and light modules, ready to be drawn.
// Made by Nayuki's QR Code generator (third_party/qrcodegen, MIT License),
// with medium error correction, so a slightly smudged print still scans.
class QrCode
{
public:
    // Empty (size 0) if the text is too long for any QR code.
    static QrCode encode(const QString &text);

    int size() const { return m_size; }          // modules per side, 0 = empty
    bool isEmpty() const { return m_size == 0; }
    bool isDark(int x, int y) const;             // false outside the grid

private:
    int m_size = 0;
    QVector<bool> m_modules; // row by row
};

} // namespace shophub
