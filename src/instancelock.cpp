/**********************************************************************
 * instancelock.cpp
 **********************************************************************
 * Copyright (C) 2026-2026 MX Authors
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 **********************************************************************/
#include "instancelock.h"

#include <QtCore/QFile>

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
QString errnoText(const char *what)
{
    return QString::fromLatin1(what) + ": " + QString::fromLocal8Bit(std::strerror(errno));
}

// Open the lock file read-only, creating it when missing. An existing file is
// opened without O_CREAT on purpose: in a sticky world-writable directory such
// as Debian's /run/lock, fs.protected_regular makes O_CREAT on another user's
// file fail even for root, which would let a GUI-created lock file lock root's
// CLI out. O_NOFOLLOW refuses a symlink planted in place of the file, and
// O_NONBLOCK keeps a FIFO planted there from hanging the open.
int openLockFile(const QByteArray &path)
{
    constexpr int baseFlags = O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK;
    for (int attempt = 0; attempt < 2; ++attempt) {
        int fd = ::open(path.constData(), baseFlags);
        if (fd >= 0 || errno != ENOENT) {
            return fd;
        }
        fd = ::open(path.constData(), baseFlags | O_CREAT | O_EXCL, 0644);
        if (fd >= 0) {
            // Readable by every user regardless of umask, or a lock file made
            // by one user would stop the others from even checking it.
            ::fchmod(fd, 0644);
            return fd;
        }
        if (errno != EEXIST) {
            return -1;
        }
        // Lost a creation race with another instance: open the file it made.
    }
    return -1;
}
} // namespace

InstanceLock::Result InstanceLock::acquire(const QString &path, QString *error)
{
    const int fd = openLockFile(QFile::encodeName(path));
    if (fd < 0) {
        if (error) {
            *error = errnoText("open");
        }
        return Result::Unavailable;
    }
    struct stat st {};
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        ::close(fd);
        if (error) {
            *error = QStringLiteral("not a regular file");
        }
        return Result::Unavailable;
    }
    int rc = 0;
    do {
        rc = ::flock(fd, LOCK_EX | LOCK_NB);
    } while (rc != 0 && errno == EINTR);
    if (rc != 0) {
        const int savedErrno = errno;
        ::close(fd);
        if (savedErrno == EWOULDBLOCK) {
            return Result::Busy;
        }
        errno = savedErrno;
        if (error) {
            *error = errnoText("flock");
        }
        return Result::Unavailable;
    }
    // Intentionally leaked: the lock lasts as long as the process.
    return Result::Acquired;
}
