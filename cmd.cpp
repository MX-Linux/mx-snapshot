#include "cmd.h"

#include <QDebug>
#include <QEventLoop>

Cmd::Cmd(QObject *parent)
    : QProcess(parent)
{
    connect(this, &Cmd::readyReadStandardOutput, [=]() { emit outputAvailable(readAllStandardOutput()); });
    connect(this, &Cmd::readyReadStandardError, [=]() { emit errorAvailable(readAllStandardError()); });
    connect(this, &Cmd::outputAvailable, [=](const QString &out) { out_buffer += out; });
    connect(this, &Cmd::errorAvailable, [=](const QString &out) { out_buffer += out; });
}

void Cmd::halt()
{
    if (state() != QProcess::NotRunning) {
        terminate();
        waitForFinished(5000);
        kill();
        waitForFinished(1000);
    }
}

bool Cmd::run(const QString &cmd, bool quiet)
{
    out_buffer.clear();
    QByteArray output;
    return run(cmd, output, quiet);
}

// util function for getting bash command output
QString Cmd::getCmdOut(const QString &cmd, bool quiet)
{
    out_buffer.clear();
    QByteArray output;
    run(cmd, output, quiet);
    return output;
}

bool Cmd::run(const QString &cmd, QByteArray &output, bool quiet)
{
    out_buffer.clear();
    connect(this, static_cast<void (QProcess::*)(int)>(&QProcess::finished), this, &Cmd::finished, Qt::UniqueConnection);
    if (this->state() != QProcess::NotRunning) {
        qDebug() << "Process already running:" << this->program() << this->arguments();
        return false;
    }
    if (!quiet) qDebug().noquote() << cmd;
    QEventLoop loop;
    connect(this, &Cmd::finished, &loop, &QEventLoop::quit, Qt::UniqueConnection);
    start("/bin/bash", QStringList() << "-c" << cmd);
    loop.exec();
    output = out_buffer.trimmed().toUtf8();
    return (exitStatus() == QProcess::NormalExit && exitCode() == 0);
}

