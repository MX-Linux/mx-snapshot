/**********************************************************************
 *  batchprocessing.cpp
 **********************************************************************
 * Copyright (C) 2020 MX Authors
 *
 * Authors: Adrian
 *          MX Linux <http://mxlinux.org>
 *
 * This file is part of MX Snapshot.
 *
 * MX Snapshot is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MX Snapshot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MX Snapshot.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#include <QDebug>
#include <QRegularExpression>

#include "batchprocessing.h"
#include "work.h"

Batchprocessing::Batchprocessing(const QCommandLineParser &arg_parser) :
    Settings(arg_parser),
    work(this)
{
    connect(qApp, &QCoreApplication::aboutToQuit, [this] { work.cleanUp(); });
    setConnections();
    QString path = snapshot_dir;
    getFreeSpaceStrings(path.remove(QRegularExpression("/snapshot$")));
    if (not arg_parser.isSet("month"))
        getUsedSpace();

    work.started = true;
    work.e_timer.start();
    if (!checkSnapshotDir() || !checkTempDir())
        work.cleanUp();
    if (not arg_parser.isSet("month"))
        work.checkEnoughSpace();
    otherExclusions();

    work.copyNewIso();
    if (!work.mkDir(snapshot_name))
        work.cleanUp();
    work.savePackageList(snapshot_name);

    if (edit_boot_menu) {
        qDebug() << QObject::tr("The program will pause the build and open the boot menu in your text editor.");
        QString cmd = getEditor() + " \"" + work_dir + "/iso-template/boot/isolinux/isolinux.cfg\"";
        shell->run(cmd);
    }
    work.setupEnv();
    work.createIso(snapshot_name);
}

Batchprocessing::~Batchprocessing()
{
}

void Batchprocessing::setConnections()
{
    connect(&timer, &QTimer::timeout, this, &Batchprocessing::progress);
    connect(shell, &Cmd::started, [this]{ timer.start(500); });
    connect(shell, &Cmd::finished, [this]{ timer.stop(); });
    connect(shell, &Cmd::outputAvailable, [](const QString &out) { qDebug().noquote() << out; });
    connect(shell, &Cmd::errorAvailable, [](const QString &out) { qWarning().noquote() << out; });
    connect(&work, &Work::message, [](const QString &out) { qDebug().noquote() << out; });
    connect(&work, &Work::messageBox, [](BoxType, const QString &title, const QString &msg) { qDebug().noquote() << title << msg; });
}

void Batchprocessing::progress()
{
    static int i = 0;

    // skip message when running mksquashfs
    if (shell->arguments().size() >= 2 && shell->arguments().at(1).startsWith("mksquashfs")) return;

    (i % 2 == 1) ? qDebug() << "\033[2KProcessing command...\r" : qDebug() << "\033[2KProcessing command\r";
    ++i;
}
