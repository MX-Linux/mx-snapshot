/**********************************************************************
 * test_filesystemutils.cpp
 **********************************************************************
 * Copyright (C) 2026-2026 MX Authors
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 **********************************************************************/
#include <QtTest/QtTest>

#include <QtCore/QDir>
#include <QtCore/QTemporaryDir>

#include "filesystemutils.h"

// The type classifier only excludes filesystems with known incompatible file
// limits. All other valid writable mounts are accepted or rejected by the
// small capability probe in isOnSupportedPartition().
class TestFileSystemUtils : public QObject
{
    Q_OBJECT

private slots:
    void usesDestinationOrNearestExistingAncestor()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString snapshot = temp.filePath("snapshot");
        QCOMPARE(FileSystemUtils::nearestExistingPath(snapshot + "/nested"), temp.path());
        QVERIFY(QDir().mkdir(snapshot));
        QCOMPARE(FileSystemUtils::nearestExistingPath(snapshot), snapshot);
        QCOMPARE(FileSystemUtils::nearestExistingPath(snapshot + "/nested"), snapshot);
    }

    void rejectsKnownIncompatibleFilesystems_data()
    {
        QTest::addColumn<QString>("type");
        for (const char *type : {"fat", "vfat", "msdos"}) {
            QTest::newRow(type) << QString(type);
        }
    }

    void rejectsKnownIncompatibleFilesystems()
    {
        QFETCH(QString, type);
        QVERIFY(FileSystemUtils::isKnownIncompatibleFilesystemType(type));
    }

    void leavesOtherFilesystemTypesToCapabilityProbe_data()
    {
        QTest::addColumn<QString>("type");
        for (const char *type : {"ext4", "exfat", "ntfs", "nfs", "cifs", "fuse.sshfs", "fuse.vmhgfs-fuse",
                                 "vboxsf", "virtiofs", "somefs", ""}) {
            QTest::newRow(type) << QString(type);
        }
    }

    void leavesOtherFilesystemTypesToCapabilityProbe()
    {
        QFETCH(QString, type);
        QVERIFY(!FileSystemUtils::isKnownIncompatibleFilesystemType(type));
    }

    void acceptsWorkingTemporaryDirectory()
    {
        QVERIFY(FileSystemUtils::isOnSupportedPartition(QDir::tempPath()));
    }

    void rejectsNonexistentPath()
    {
        QVERIFY(!FileSystemUtils::isOnSupportedPartition(QStringLiteral("/nonexistent/path/for/test")));
    }
};

QTEST_GUILESS_MAIN(TestFileSystemUtils)
#include "test_filesystemutils.moc"
