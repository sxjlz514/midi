/**
 * @file ocfwriter.h
 * @brief OCF v1 位图流写出（与 ocffile.cpp 的读取校验规则保持一致）。
 */

#ifndef OCFWRITER_H
#define OCFWRITER_H

#include "ocffile.h"

#include <QByteArray>
#include <QString>

class OcfWriter
{
public:
    /** 组包并写盘。 */
    static bool writeFile(const QString &path, const OcfData &data, QString &error);

    /** 只组包，不写盘；失败时 error 非空。 */
    static QByteArray encode(const OcfData &data, QString &error);
};

#endif // OCFWRITER_H
