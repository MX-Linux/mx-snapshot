/**********************************************************************
 * test_instancelock.cpp
 **********************************************************************
 * Copyright (C) 2026-2026 MX Authors
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 **********************************************************************/
#include <QtTest/QtTest>

#include <QtCore/QFile>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>

#include <sys/stat.h>

#include "instancelock.h"

// flock() locks belong to an open file description, so a second acquire() in
// the same process opens a new one and conflicts exactly as another process
// would.
class TestInstanceLock : public QObject
{
    Q_OBJECT

private slots:
    void secondAcquireIsBusy()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("test.lock");
        QCOMPARE(InstanceLock::acquire(path), InstanceLock::Result::Acquired);
        QCOMPARE(InstanceLock::acquire(path), InstanceLock::Result::Busy);
    }

    void createsWorldReadableFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("test.lock");
        const mode_t oldMask = ::umask(077);
        const auto result = InstanceLock::acquire(path);
        ::umask(oldMask);
        QCOMPARE(result, InstanceLock::Result::Acquired);
        struct stat st {};
        QCOMPARE(::stat(QFile::encodeName(path).constData(), &st), 0);
        QCOMPARE(st.st_mode & 0777, mode_t(0644));
    }

    void reusesExistingReadOnlyFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("test.lock");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther));
        QCOMPARE(InstanceLock::acquire(path), InstanceLock::Result::Acquired);
    }

    void refusesSymlink()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString target = dir.filePath("target");
        QFile file(target);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const QString path = dir.filePath("test.lock");
        QVERIFY(QFile::link(target, path));
        QString error;
        QCOMPARE(InstanceLock::acquire(path, &error), InstanceLock::Result::Unavailable);
        QVERIFY(!error.isEmpty());
    }

    // A FIFO in place of the file must be refused without blocking the open.
    void refusesFifo()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("test.lock");
        QCOMPARE(::mkfifo(QFile::encodeName(path).constData(), 0644), 0);
        QString error;
        QCOMPARE(InstanceLock::acquire(path, &error), InstanceLock::Result::Unavailable);
        QCOMPARE(error, QStringLiteral("not a regular file"));
    }

    void missingDirectoryIsUnavailable()
    {
        QCOMPARE(InstanceLock::acquire("/nonexistent-dir-for-test/test.lock"), InstanceLock::Result::Unavailable);
    }

    // The lock must not leak into children: a detached editor or other
    // long-lived child would otherwise keep "another snapshot" running.
    void lockIsNotInheritedByChildren()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("test.lock");
        QCOMPARE(InstanceLock::acquire(path), InstanceLock::Result::Acquired);
        // The child lists its open descriptors; none may point at the lock file.
        QProcess child;
        child.start("/bin/sh", {"-c", "ls -l /proc/self/fd/"});
        QVERIFY(child.waitForFinished());
        QVERIFY(!QString::fromLocal8Bit(child.readAllStandardOutput()).contains(path));
    }
};

QTEST_GUILESS_MAIN(TestInstanceLock)
#include "test_instancelock.moc"
