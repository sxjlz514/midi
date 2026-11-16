/**
 * @file ocfwriter.cpp
 * @brief 写 OCF v1：24 字节头 + 12 字节帧 + CRC32。
 */

#include "ocfwriter.h"

#include <QFile>
#include <QtEndian>

namespace {

constexpr int kHeaderSize = 24;
constexpr int kFrameSize = 12;

void appendLe16(QByteArray &out, quint16 value)
{
    const quint16 le = qToLittleEndian(value);
    out.append(reinterpret_cast<const char *>(&le), 2);
}

void appendLe32(QByteArray &out, quint32 value)
{
    const quint32 le = qToLittleEndian(value);
    out.append(reinterpret_cast<const char *>(&le), 4);
}

void appendLe64(QByteArray &out, quint64 value)
{
    const quint64 le = qToLittleEndian(value);
    out.append(reinterpret_cast<const char *>(&le), 8);
}

quint32 crc32(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFFu;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<quint8>(data.at(i));
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

} // namespace

QByteArray OcfWriter::encode(const OcfData &data, QString &error)
{
    error.clear();
    const int span = static_cast<int>(data.rangeEnd) - static_cast<int>(data.rangeStart) + 1;
    if (data.rangeStart > data.rangeEnd || data.rangeEnd > 127) {
        error = QStringLiteral("音高范围必须从低到高且不超过 127。");
        return QByteArray();
    }
    if (span > 32) {
        error = QStringLiteral("音高跨度不能超过 32（32 位位图）。");
        return QByteArray();
    }
    if (data.aggregationWindowMs == 0) {
        error = QStringLiteral("聚合窗不能为 0。");
        return QByteArray();
    }
    if (data.mappingMode > 1) {
        error = QStringLiteral("映射模式只能是 range 或 wrap。");
        return QByteArray();
    }

    const quint32 allowedMask = span == 32 ? 0xFFFFFFFFu : ((1u << span) - 1u);
    QByteArray out;
    out.reserve(kHeaderSize + data.frames.size() * kFrameSize + 4);
    out.append("OCF1", 4);
    out.append(static_cast<char>(data.version == 0 ? 1 : data.version));
    out.append(static_cast<char>(data.bitmapBits == 0 ? 32 : data.bitmapBits));
    out.append(static_cast<char>(data.mappingMode));
    out.append(static_cast<char>(data.rangeStart));
    out.append(static_cast<char>(data.rangeEnd));
    out.append(static_cast<char>(0));
    appendLe16(out, data.aggregationWindowMs);
    appendLe64(out, static_cast<quint64>(data.durationMs));
    appendLe32(out, static_cast<quint32>(data.frames.size()));

    quint64 previousTime = 0;
    for (int i = 0; i < data.frames.size(); ++i) {
        const OcfFrame &frame = data.frames.at(i);
        if (i > 0 && frame.timeMs <= previousTime) {
            error = QStringLiteral("第 %1 帧时间未递增。").arg(i);
            return QByteArray();
        }
        if ((frame.bitmap & ~allowedMask) != 0) {
            error = QStringLiteral("第 %1 帧位图超出当前音高跨度。").arg(i);
            return QByteArray();
        }
        appendLe64(out, static_cast<quint64>(frame.timeMs));
        appendLe32(out, frame.bitmap);
        previousTime = frame.timeMs;
    }

    appendLe32(out, crc32(out));
    return out;
}

bool OcfWriter::writeFile(const QString &path, const OcfData &data, QString &error)
{
    const QByteArray bytes = encode(data, error);
    if (bytes.isEmpty()) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = QStringLiteral("无法写入 OCF：%1").arg(file.errorString());
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        error = QStringLiteral("写入 OCF 不完整。");
        return false;
    }
    return true;
}
