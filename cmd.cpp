#include <QDebug>
#include <QEventLoop>

#include "cmd.h"

Cmd::Cmd(QObject *parent)
    : QProcess(parent)
{
    connect(this, &Cmd::readyReadStandardOutput, [this] { emit outputAvailable(readAllStandardOutput()); });
    connect(this, &Cmd::readyReadStandardError, [this] { emit errorAvailable(readAllStandardError()); });
    connect(this, &Cmd::outputAvailable, [this](const QString &out) { out_buffer += out; });
    connect(this, &Cmd::errorAvailable, [this](const QString &out) { out_buffer += out; });
}

void Cmd::halt()
{
    halting = true;
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
    QString output;
    return run(cmd, output, quiet);
}

// util function for getting bash command output
QString Cmd::getCmdOut(const QString &cmd, bool quiet)
{
    out_buffer.clear();
    QString output;
    run(cmd, output, quiet);
    return output;
}

bool Cmd::run(const QString &cmd, QString &output, bool quiet)
{
    if (halting) {
        qDebug() << "Halting, cmd not executed:" << cmd;
        return false;
    }
    out_buffer.clear();
    connect(this, QOverload<int>::of(&QProcess::finished), this, &Cmd::finished, Qt::UniqueConnection);
    if (this->state() != QProcess::NotRunning) {
        qDebug() << "Process already running:" << this->program() << this->arguments();
        return false;
    }
    if (!quiet) qDebug().noquote() << cmd;
    QEventLoop loop;
    connect(this, &Cmd::finished, &loop, &QEventLoop::quit, Qt::UniqueConnection);
    start("/bin/bash", QStringList() << "-c" << cmd);
    loop.exec();
    output = out_buffer.trimmed();
    return (exitStatus() == QProcess::NormalExit && exitCode() == 0);
}

