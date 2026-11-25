#pragma once

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>

class SystemInfo
{
public:
    // Check if running on 32-bit architecture
    [[nodiscard]] static bool is386();

    // Check if running from a live environment
    [[nodiscard]] static bool isLive();

    // Warn when the non-recursive root bind omits a mounted system directory.
    [[nodiscard]] static QString systemMountWarning();

    [[nodiscard]] static QMap<QString, QString> listUserHomes();

    // Read kernel boot options from script
    [[nodiscard]] static QString readKernelOpts();
};
