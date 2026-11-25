#pragma once

#include <QtCore/QString>
#include <QtCore/QSet>

class FileSystemUtils
{
public:
    [[nodiscard]] static QString nearestExistingPath(const QString &path);

    // Get free space in KiB for a given path
    [[nodiscard]] static quint64 getFreeSpace(const QString &path);

    // Get formatted free space string for display
    [[nodiscard]] static QString getFreeSpaceString(const QString &path);

    // Check whether a directory can host the snapshot work dir. The filesystem
    // must be valid, ready, writable, and pass a small POSIX capability probe.
    [[nodiscard]] static bool isOnSupportedPartition(const QString &dir);

    // Filesystems with a known maximum file size incompatible with ISO and
    // squashfs artifacts. Other filesystem types are assessed by the probe.
    [[nodiscard]] static bool isKnownIncompatibleFilesystemType(const QString &type);

    // Return directory with larger free space between two options
    [[nodiscard]] static QString largerFreeSpace(const QString &dir1, const QString &dir2);

    // Return directory with largest free space among three options
    [[nodiscard]] static QString largerFreeSpace(const QString &dir1, const QString &dir2, const QString &dir3);

private:
    [[nodiscard]] static bool supportsWorkDirectory(const QString &dir);

    static const QSet<QString> incompatiblePartitions;
};
