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
#include <QLocale>
#include <QScopedPointer>
#include <QTranslator>

#include <unistd.h>
#include "mainwindow.h"
#include "version.h"

static QScopedPointer<QFile> logFile;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
void printHelp();

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationVersion(VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::tr("Tool used for creating a live-CD from the running system"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({{{"m", "month"}, QApplication::tr("Create a montly snapshot, add 'Month' name in the ISO name, skip used space calculation")},
                       {{"p", "preempt"}, QApplication::tr("Option to fix issue with calculating checksums on preempt_rt kernels")}});
    parser.process(app);

    app.setWindowIcon(QIcon::fromTheme("mx-snapshot"));

    QTranslator appTran;
    appTran.load(QString("mx-snapshot_") + QLocale::system().name(), "/usr/share/mx-snapshot/locale");
    app.installTranslator(&appTran);

    // Check if SQUASHFS is available
    if (system("[ -f /boot/config-$(uname -r) ]") == 0 && system("grep -q ^CONFIG_SQUASHFS=[ym] /boot/config-$(uname -r)") != 0) {
        QMessageBox::critical(nullptr, QApplication::tr("Error"),
                QApplication::tr("Current kernel doesn't support Squashfs, cannot continue."));
        return EXIT_FAILURE;
    }

    if (getuid() == 0) {
        QString log_name= "/var/log/mx-snapshot.log";
        // archive old log
        system("[ -f " + log_name.toUtf8() + " ] && mv " + log_name.toUtf8() + " " + log_name.toUtf8() + ".old");
        // Set the logging files
        logFile.reset(new QFile(log_name));
        // Open the file logging
        logFile.data()->open(QFile::Append | QFile::Text);
        // Set handler
        qInstallMessageHandler(messageHandler);

        qDebug().noquote() << app.applicationName() << QApplication::tr("version:") << app.applicationVersion();
        MainWindow w(nullptr, parser);
        w.show();
        return app.exec();
    } else {
        system("su-to-root -X -c " + QCoreApplication::applicationFilePath().toUtf8() + "&");
    }
}

// The implementation of the handler
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Write to terminal
    QTextStream term_out(stdout);
    term_out << msg << endl;

    // Open stream file writes
    QTextStream out(logFile.data());

    // Write the date of recording
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ");
    // By type determine to what level belongs message
    switch (type)
    {
    case QtInfoMsg:     out << "INF "; break;
    case QtDebugMsg:    out << "DBG "; break;
    case QtWarningMsg:  out << "WRN "; break;
    case QtCriticalMsg: out << "CRT "; break;
    case QtFatalMsg:    out << "FTL "; break;
    }
    // Write to the output category of the message and the message itself
    out << context.category << ": "
        << msg << endl;
    out.flush();    // Clear the buffered data
}


