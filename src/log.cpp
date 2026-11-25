/**********************************************************************
 * Log.cpp
 **********************************************************************
 * Copyright (C) 2023-2026 MX Authors
 *
 * Authors: Adrian <adrian@mxlinux.org>
 *          MX Linux <http://mxlinux.org>
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package. If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/
#include "log.h"

#include <QDateTime>
#include <QFileInfo>

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "cmd.h"

Log::Log(const QString &fileName)
{
    logPath = fileName;

    // Check if log file exists and has wrong ownership
    if (QFileInfo::exists(fileName)) {
        fixLogFileOwnership(fileName);
    }

    if (!openLogFile()) {
        qWarning() << "Could not open log file:" << fileName;
    } else {
        qInstallMessageHandler(Log::messageHandler);
    }
}

QString Log::defaultLogPath(const QString &appName)
{
    // Keep the log out of world-writable /tmp. As root use /run (root-only);
    // as the user use the private per-user runtime dir ($XDG_RUNTIME_DIR,
    // i.e. /run/user/<uid>, mode 0700). Fall back to /tmp only if no runtime
    // dir is available (openLogFile() still refuses to follow a symlink there).
    if (geteuid() == 0) {
        return "/run/" + appName + ".log";
    }
    const QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!runtimeDir.isEmpty() && QFileInfo(runtimeDir).isDir()) {
        return runtimeDir + '/' + appName + ".log";
    }
    return "/tmp/" + appName + ".log";
}

bool Log::openLogFile()
{
    // logPath, not logFile.fileName(): QFile::open(int fd) clears the file name.
    const QByteArray name = logPath.toLocal8Bit();
    // O_NOFOLLOW: never follow a symlink at the final path component, so even
    // the world-writable /tmp fallback cannot be redirected at another file.
    const int fd = ::open(name.constData(), O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC,
                          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        return false;
    }
    if (!logFile.open(fd, QIODevice::Append | QIODevice::Text, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        return false;
    }
    return true;
}

void Log::messageHandler(QtMsgType type, [[maybe_unused]] const QMessageLogContext &context, const QString &msg)
{
    // Reentrancy guard: if a qWarning() inside this handler triggers another
    // dispatch (Qt does this on some message types), bail to plain stderr to
    // avoid infinite recursion.
    static thread_local bool inHandler = false;
    if (inHandler) {
        std::fputs((msg + '\n').toLocal8Bit().constData(), stderr);
        return;
    }
    inHandler = true;

    QTextStream termOut(stdout);

    // Check if the message contains carriage return or starts with the escape sequence for clearing the line
    if (msg.contains('\r') || msg.startsWith("\033[2K")) {
        termOut << msg;
        inHandler = false;
        return; // Skip writing to the log file
    }

    termOut << msg << '\n';
    appendToFile(type, msg);
    inHandler = false;
}

void Log::appendToFile(QtMsgType type, const QString &msg)
{
    if (!logFile.isOpen()) {
        // Try to fix ownership and reopen.
        fixLogFileOwnership(logPath);
        if (!openLogFile()) {
            qWarning() << "Still could not open log file after ownership fix";
            return;
        }
    }

    QTextStream out(&logFile);
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ");

    switch (type) {
    case QtInfoMsg:
        out << "INF";
        break;
    case QtDebugMsg:
        out << "DBG";
        break;
    case QtWarningMsg:
        out << "WRN";
        break;
    case QtCriticalMsg:
        out << "CRT";
        break;
    case QtFatalMsg:
        out << "FTL";
        break;
    }

    out << ": " << msg << '\n';
}

QString Log::getLog()
{
    return logPath;
}

void Log::fixLogFileOwnership(const QString &fileName)
{
    const QFileInfo fileInfo(fileName);
    if (!fileInfo.exists()) {
        return;
    }

    // Get current user information
    const uid_t currentUid = getuid();

    // Get file ownership and permissions
    struct stat fileStat;
    const QByteArray fileNameBytes = fileName.toLocal8Bit();
    // Never act on a symlink: a chown here follows the link, so a planted
    // symlink could otherwise hand ownership of an arbitrary file to the user.
    if (lstat(fileNameBytes.constData(), &fileStat) != 0 || S_ISLNK(fileStat.st_mode)) {
        return;
    }
    if (stat(fileNameBytes.constData(), &fileStat) != 0) {
        return;
    }

    // Case 1: Running as regular user, but file is owned by root.
    // No qDebug — fixLogFileOwnership runs from inside the message handler.
    if (fileStat.st_uid == 0 && currentUid != 0) {
        Cmd cmd;
        const QString username = Cmd::loggedInUserName();
        if (!username.isEmpty()) {
            cmd.procAsRoot("chown", {username + ":", fileName}, nullptr, nullptr, Cmd::QuietMode::Yes);
        }
    }
    // Case 2: Running as root, but file is owned by regular user with restrictive permissions
    else if (fileStat.st_uid != 0 && currentUid == 0) {
        if (chown(fileNameBytes.constData(), 0, 0) == 0) {
            chmod(fileNameBytes.constData(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        }
    }
}
