/**********************************************************************
 *  main.cpp
 **********************************************************************
 * Copyright (C) 2015 MX Authors
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

#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDebug>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QScopedPointer>
#include <QTranslator>

#include <unistd.h>
#include "mainwindow.h"
#include "version.h"

static QScopedPointer<QFile> logFile;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationVersion(VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Tool used for creating a live-CD from the running system"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({{{"m", "month"}, QObject::tr("Create a montly snapshot, add 'Month' name in the ISO name, skip used space calculation")},
                       {{"p", "preempt"}, QObject::tr("Option to fix issue with calculating checksums on preempt_rt kernels")}});
    parser.process(app);

    app.setWindowIcon(QIcon::fromTheme(app.applicationName()));

    QTranslator qtTran;
    if (qtTran.load(QLocale::system(), "qt", "_", QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTran);

    QTranslator qtBaseTran;
    if (qtBaseTran.load("qtbase_" + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtBaseTran);

    QTranslator appTran;
    if (appTran.load(app.applicationName() + "_" + QLocale::system().name(), "/usr/share/" + app.applicationName() + "/locale"))
        app.installTranslator(&appTran);

    // Check if SQUASHFS is available
    if (system("[ -f /boot/config-$(uname -r) ]") == 0 && system("grep -q ^CONFIG_SQUASHFS=[ym] /boot/config-$(uname -r)") != 0) {
        QMessageBox::critical(nullptr, QObject::tr("Error"),
                QObject::tr("Current kernel doesn't support Squashfs, cannot continue."));
        return EXIT_FAILURE;
    }

    if (getuid() == 0) {
        QString log_name= "/var/log/" + app.applicationName() + ".log";
        system("[ -f " + log_name.toUtf8() + " ] && mv " + log_name.toUtf8() + " " + log_name.toUtf8() + ".old");
        logFile.reset(new QFile(log_name));
        logFile.data()->open(QFile::Append | QFile::Text);
        qInstallMessageHandler(messageHandler);

        qDebug().noquote() << app.applicationName() << QObject::tr("version:") << app.applicationVersion();
        MainWindow w(nullptr, parser);
        w.show();
        return app.exec();
    } else {
        system("su-to-root -X -c " + app.applicationFilePath().toUtf8() + "&");
    }
}

// The implementation of the handler
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QTextStream term_out(stdout);
    term_out << msg << QStringLiteral("\n");

    QTextStream out(logFile.data());
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ");
    switch (type)
    {
    case QtInfoMsg:     out << QStringLiteral("INF "); break;
    case QtDebugMsg:    out << QStringLiteral("DBG "); break;
    case QtWarningMsg:  out << QStringLiteral("WRN "); break;
    case QtCriticalMsg: out << QStringLiteral("CRT "); break;
    case QtFatalMsg:    out << QStringLiteral("FTL "); break;
    }
    out << context.category << QStringLiteral(": ") << msg << endl;
}


