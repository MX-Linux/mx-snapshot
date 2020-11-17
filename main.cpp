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

#include "mainwindow.h"
#include <unistd.h>
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QIcon>
#include <QScopedPointer>
#include <QDateTime>
#include <QDebug>

static QScopedPointer<QFile> logFile;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
void printHelp();

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    if (a.arguments().contains("--help") || a.arguments().contains("-h") ) {
        printHelp();
        return EXIT_SUCCESS;
    }
    if (a.arguments().contains("--version") || a.arguments().contains("-v") ) {
       system("echo 'Installer version'; dpkg-query -f '${Version}' -W mx-snapshot; echo");
       return EXIT_SUCCESS;
    }

    a.setWindowIcon(QIcon::fromTheme("mx-snapshot"));

    QString log_name= "/var/log/mx-snapshot.log";
    // archive old log
    system("[ -f " + log_name.toUtf8() + " ] && mv " + log_name.toUtf8() + " " + log_name.toUtf8() + ".old");
    // Set the logging files
    logFile.reset(new QFile(log_name));
    // Open the file logging
    logFile.data()->open(QFile::Append | QFile::Text);
    // Set handler
    qInstallMessageHandler(messageHandler);

    QTranslator qtTran;
    qtTran.load(QString("qt_") + QLocale::system().name());
    a.installTranslator(&qtTran);

    QTranslator appTran;
    appTran.load(QString("mx-snapshot_") + QLocale::system().name(), "/usr/share/mx-snapshot/locale");
    a.installTranslator(&appTran);

    // Check if SQUASHFS is available
    if (system("[ -f /boot/config-$(uname -r) ]") == 0 && system("grep -q ^CONFIG_SQUASHFS=[ym] /boot/config-$(uname -r)") != 0) {
        QMessageBox::critical(nullptr, QApplication::tr("Error"),
                QApplication::tr("Current kernel doesn't support Squashfs, cannot continue."));
        return EXIT_FAILURE;
    }

    if (getuid() == 0) {
        MainWindow w(nullptr, a.arguments());
        w.show();
        return a.exec();
    } else {
        system("su-to-root -X -c " + QCoreApplication::applicationFilePath().toUtf8() + "&");
//        QApplication::beep();
//        QMessageBox::critical(nullptr, QApplication::tr("Error"),
//                              QApplication::tr("You must run this program as root."));
//        return EXIT_FAILURE;
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

// print CLI help info
void printHelp()
{
    qDebug() << "Usage: mx-snapshot [<options>]\n";
    qDebug() << "Options:";
    qDebug() << "  -m --monthly Month   Create a montly snapshot, add 'Month' in the ISO name, skip used space calculation";
    qDebug() << "  -v --version         Show version information";
}

