#include "filesystemutils.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QStorageInfo>
#include <QtCore/QTemporaryDir>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

const QSet<QString> FileSystemUtils::incompatiblePartitions = {
    // These FAT variants cannot hold an ISO or squashfs file larger than 4 GiB.
    "fat", "vfat", "msdos",
};

QString FileSystemUtils::nearestExistingPath(const QString &path)
{
    QString candidate = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    while (!QFileInfo::exists(candidate)) {
        const QString parent = QFileInfo(candidate).absolutePath();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }
    return candidate;
}

quint64 FileSystemUtils::getFreeSpace(const QString &path)
{
    QStorageInfo storage(path + "/");
    if (!storage.isReady() || storage.isReadOnly()) {
        qDebug() << "Cannot determine free space for" << path
                 << ": Drive not ready, or does not exist, or is read-only.";
        return 0;
    }
    return storage.bytesAvailable() / 1024;
}

QString FileSystemUtils::getFreeSpaceString(const QString &path)
{
    constexpr float factor = 1024 * 1024;
    const quint64 freeSpace = getFreeSpace(path);
    return QString::number(static_cast<double>(freeSpace) / factor, 'f', 2) + "GiB";
}

bool FileSystemUtils::isOnSupportedPartition(const QString &dir)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    const QStorageInfo storage(dir + "/");
    if (!storage.isValid() || !storage.isReady() || storage.isReadOnly()) {
        qDebug() << "Rejecting" << dir << ": storage not valid, not ready, or read-only";
        return false;
    }
    const QString partType = storage.fileSystemType();
    if (isKnownIncompatibleFilesystemType(partType)) {
        qDebug() << "Rejecting" << dir << ": filesystem" << partType << "has an incompatible file-size limit";
        return false;
    }
    const bool isSupported = supportsWorkDirectory(dir);
    qDebug() << "Detected partition:" << partType << "Supported part:" << isSupported;
    return isSupported;
}

bool FileSystemUtils::isKnownIncompatibleFilesystemType(const QString &type)
{
    return incompatiblePartitions.contains(type);
}

bool FileSystemUtils::supportsWorkDirectory(const QString &dir)
{
    QTemporaryDir probe(QDir(dir).filePath(".mx-snapshot-check-XXXXXX"));
    if (!probe.isValid()) {
        qDebug() << "Rejecting" << dir << ": could not create a private probe directory";
        return false;
    }

    const QString dataPath = probe.filePath("data");
    const QByteArray dataPathBytes = QFile::encodeName(dataPath);
    const QByteArray contents("mx-snapshot");
    QFile data(dataPath);
    if (!data.open(QIODevice::WriteOnly) || data.write(contents) != contents.size() || !data.flush()) {
        qDebug() << "Rejecting" << dir << ": probe file write failed";
        return false;
    }
    data.close();

    if (!data.open(QIODevice::ReadOnly) || data.readAll() != contents) {
        qDebug() << "Rejecting" << dir << ": probe file read failed";
        return false;
    }
    data.close();

    // chmod fidelity is not load-bearing for the snapshot: the ISO template is
    // extracted unprivileged and the live system's permissions come from the
    // squashfs (recorded off bind-root), not from work-dir modes. Filesystems
    // that silently ignore chmod (ntfs-3g, exfat) still host a working work
    // dir, so note the deviation without rejecting.
    constexpr mode_t probeMode = S_IRUSR | S_IWUSR | S_IXUSR;
    struct stat dataStat {};
    if (::chmod(dataPathBytes.constData(), probeMode) != 0) {
        qDebug() << "Note:" << dir << ": probe chmod failed:" << std::strerror(errno);
    } else if (::stat(dataPathBytes.constData(), &dataStat) != 0 || (dataStat.st_mode & 0777) != probeMode) {
        qDebug() << "Note:" << dir << ": probe chmod did not preserve the requested mode";
    }

    const QString renamedPath = probe.filePath("renamed");
    const QByteArray renamedPathBytes = QFile::encodeName(renamedPath);
    if (::rename(dataPathBytes.constData(), renamedPathBytes.constData()) != 0) {
        qDebug() << "Rejecting" << dir << ": probe rename failed:" << std::strerror(errno);
        return false;
    }

    const QString nestedDir = probe.filePath("nested/directory");
    if (!QDir().mkpath(nestedDir)) {
        qDebug() << "Rejecting" << dir << ": probe nested-directory creation failed";
        return false;
    }
    return true;
}

QString FileSystemUtils::largerFreeSpace(const QString &dir1, const QString &dir2)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (QStorageInfo(dir1 + "/").device() == QStorageInfo(dir2 + "/").device()) {
        return dir1;
    }
    const quint64 dir1Free = getFreeSpace(dir1);
    const quint64 dir2Free = getFreeSpace(dir2);
    return dir1Free >= dir2Free ? dir1 : dir2;
}

QString FileSystemUtils::largerFreeSpace(const QString &dir1, const QString &dir2, const QString &dir3)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    return largerFreeSpace(largerFreeSpace(dir1, dir2), dir3);
}
