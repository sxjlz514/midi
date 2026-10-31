/**
 * @file ocffile.cpp
 * @brief Validates and reads OCF v1 binary bitmap streams.
 */

#include "ocffile.h"

#include <QFile>
#include <QtEndian>
#include <limits>

namespace {

constexpr int kHeaderSize = 24;
constexpr int kFrameSize = 12;
constexpr int kCrcSize = 4;

quint32 crc32(const QByteArray &data, int length)
{
    quint32 crc = 0xFFFFFFFFu;
    for (int i = 0; i < length; ++i) {
        crc ^= static_cast<quint8>(data.at(i));
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

} // namespace

bool OcfFile::readFile(const QString &path, OcfData &out, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("无法打开 OCF 文件：%1").arg(file.errorString());
        return false;
    }
    return parse(file.readAll(), out, error);
}

bool OcfFile::parse(const QByteArray &bytes, OcfData &out, QString &error)
{
    if (bytes.size() < kHeaderSize + kCrcSize) {
        error = QStringLiteral("OCF 文件过短");
        return false;
    }
    if (bytes.left(4) != QByteArrayLiteral("OCF1")) {
        error = QStringLiteral("OCF 文件头标识错误");
        return false;
    }

    const auto *raw = reinterpret_cast<const uchar *>(bytes.constData());
    OcfData parsed;
    parsed.version = raw[4];
    parsed.bitmapBits = raw[5];
    parsed.mappingMode = raw[6];
    parsed.rangeStart = raw[7];
    parsed.rangeEnd = raw[8];
    const quint8 reserved = raw[9];
    parsed.aggregationWindowMs = qFromLittleEndian<quint16>(raw + 10);
    parsed.durationMs = qFromLittleEndian<quint64>(raw + 12);
    const quint32 frameCount = qFromLittleEndian<quint32>(raw + 20);

    if (parsed.version != 1 || parsed.bitmapBits != 32 || parsed.mappingMode > 1
        || reserved != 0 || parsed.rangeStart > parsed.rangeEnd
        || parsed.rangeEnd > 127 || parsed.aggregationWindowMs == 0
        || (parsed.rangeEnd - parsed.rangeStart + 1) > 32
        || parsed.durationMs > static_cast<quint64>(std::numeric_limits<qint64>::max())) {
        error = QStringLiteral("OCF 文件头包含不支持或非法的参数");
        return false;
    }
    const quint64 expectedSize = static_cast<quint64>(kHeaderSize)
        + static_cast<quint64>(frameCount) * kFrameSize + kCrcSize;
    if (expectedSize != static_cast<quint64>(bytes.size())) {
        error = QStringLiteral("OCF 记录数量与文件长度不一致");
        return false;
    }
    const quint32 storedCrc = qFromLittleEndian<quint32>(raw + bytes.size() - kCrcSize);
    if (storedCrc != crc32(bytes, bytes.size() - kCrcSize)) {
        error = QStringLiteral("OCF CRC32 校验失败，文件可能已损坏");
        return false;
    }

    const int span = parsed.rangeEnd - parsed.rangeStart + 1;
    const quint32 allowedMask = span == 32 ? 0xFFFFFFFFu : ((1u << span) - 1u);
    quint64 previousTime = 0;
    parsed.frames.reserve(static_cast<qsizetype>(frameCount));
    int pos = kHeaderSize;
    for (quint32 i = 0; i < frameCount; ++i, pos += kFrameSize) {
        OcfFrame frame;
        frame.timeMs = qFromLittleEndian<quint64>(raw + pos);
        frame.bitmap = qFromLittleEndian<quint32>(raw + pos + 8);
        if ((i > 0 && frame.timeMs <= previousTime)
            || frame.timeMs > parsed.durationMs || (frame.bitmap & ~allowedMask) != 0) {
            error = QStringLiteral("OCF 第 %1 条位图记录的时间或键位非法").arg(i);
            return false;
        }
        previousTime = frame.timeMs;
        parsed.frames.push_back(frame);
    }

    out = std::move(parsed);
    error.clear();
    return true;
}

QString OcfFile::toText(const OcfData &data, const QString &sourceName, bool crcValid)
{
    const QString mode = data.mappingMode == 0
        ? QStringLiteral("range（范围外忽略）")
        : QStringLiteral("wrap（按范围循环）");
    QString text;
    text += QStringLiteral("OCF 位图流 v%1\n").arg(data.version);
    text += QStringLiteral("文件：%1\n").arg(sourceName);
    text += QStringLiteral("校验：CRC32 %1\n").arg(crcValid ? QStringLiteral("通过")
                                                               : QStringLiteral("失败"));
    text += QStringLiteral("位宽：%1\n映射：%2 %3..%4（首尾包含）\n")
                .arg(data.bitmapBits).arg(mode).arg(data.rangeStart).arg(data.rangeEnd);
    text += QStringLiteral("聚合窗：%1 ms\n总时长：%2 ms\n记录数：%3\n\n")
                .arg(data.aggregationWindowMs).arg(data.durationMs).arg(data.frames.size());
    text += QStringLiteral("序号\t时间(ms)\t位图(hex)\t按下的通道\n");
    for (int i = 0; i < data.frames.size(); ++i) {
        const OcfFrame &frame = data.frames.at(i);
        const QString bitmapText = QString::number(frame.bitmap, 16)
                                       .rightJustified(8, QLatin1Char('0')).toUpper();
        QStringList active;
        for (int bit = 0; bit < 32; ++bit) {
            if ((frame.bitmap >> bit) & 1u) {
                active.push_back(QString::number(bit + 1));
            }
        }
        text += QStringLiteral("%1\t%2\t0x%3\t%4\n")
                    .arg(i, 4, 10, QLatin1Char('0'))
                    .arg(frame.timeMs)
                    .arg(bitmapText)
                    .arg(active.isEmpty() ? QStringLiteral("无") : active.join(QLatin1Char(',')));
    }
    return text;
}
