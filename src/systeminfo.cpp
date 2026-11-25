#include "systeminfo.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QProcess>
#include <QtCore/QSysInfo>

#include "cmd.h"

bool SystemInfo::is386()
{
    return QSysInfo::currentCpuArchitecture() == "i386";
}

bool SystemInfo::isLive()
{
    if (QProcess::execute("mountpoint", {"-q", "/run/archiso/bootmnt"}) == 0) {
        return true;
    }
    return QProcess::execute("mountpoint", {"-q", "/live/aufs"}) == 0;
}

QString SystemInfo::systemMountWarning()
{
    QFile mounts("/proc/self/mountinfo");
    if (!mounts.open(QIODevice::ReadOnly)) {
        return QObject::tr("Could not determine whether /usr and /var are separate mounts.");
    }
    QStringList omitted;
    // Field 5 is the mount point. mountinfo includes same-device bind mounts.
    const QByteArray contents = mounts.readAll();
    if (contents.isEmpty() || mounts.error() != QFileDevice::NoError) {
        return QObject::tr("Could not determine whether /usr and /var are separate mounts.");
    }
    for (const QByteArray &line : contents.split('\n')) {
        const QByteArray mount = line.split(' ').value(4);
        if (mount == "/usr" || mount == "/var") {
            omitted << QString::fromLatin1(mount);
        }
    }
    omitted.removeDuplicates();
    return omitted.isEmpty() ? QString()
                             : QObject::tr("The snapshot will omit the mounted contents of %1. "
                                           "The resulting ISO may be unusable.").arg(omitted.join(", "));
}

QMap<QString, QString> SystemInfo::listUserHomes()
{
    const QStringList lines = Cmd().getOut("lslogins --noheadings -u -o user,HOMEDIR", Cmd::QuietMode::Yes)
                                  .split('\n', Qt::SkipEmptyParts);
    QMap<QString, QString> homes;
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        const int sep = trimmed.indexOf(' ');
        const QString home = QDir::cleanPath(trimmed.mid(sep + 1).trimmed());
        if (sep > 0 && home.startsWith("/home/")) {
            homes.insert(trimmed.left(sep), home);
        }
    }
    return homes;
}

QString SystemInfo::readKernelOpts()
{
    const QString script = QString("/usr/share/%1/scripts/snapshot-bootparameter.sh")
                               .arg(QCoreApplication::applicationName());
    return Cmd().getOut(script).replace('\n', ' ');
}
