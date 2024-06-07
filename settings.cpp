/**********************************************************************
 *  settings.cpp
 **********************************************************************
 * Copyright (C) 2020-2024 MX Authors
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

#include "settings.h"

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QStorageInfo>

#include <unistd.h>

#ifndef CLI_BUILD
#include <QMessageBox>
#endif

Settings::Settings(const QCommandLineParser &arg_parser)
    : config_file("/etc/" + qApp->applicationName() + ".conf")
{
    if (QFileInfo::exists("/tmp/installed-to-live/cleanup.conf")) { // Cleanup installed-to-live from other sessions
        QString elevate {QFile::exists("/usr/bin/pkexec") ? "/usr/bin/pkexec" : "/usr/bin/gksu"};
        Cmd().run(elevate + " /usr/lib/" + QCoreApplication::applicationName() + "/snapshot-lib cleanup");
    }
    loadConfig(); // Load settings from .conf file
    setVariables();
    processArgs(arg_parser);
    if (arg_parser.isSet("month")) {
        monthly = true;
        setMonthlySnapshot(arg_parser);
    }
    override_size = arg_parser.isSet("override-size");
    processExclArgs(arg_parser);
}

// Check if compression is available in the kernel (lz4, lzo, xz)
bool Settings::checkCompression() const
{
    if (compression == "gzip") { // Don't check for gzip
        return true;
    }
    if (!QFileInfo::exists("/boot/config-" + kernel)) { // Return true if cannot check config file
        return true;
    }
    return (Cmd().run("grep ^CONFIG_SQUASHFS_" + compression.toUpper() + "=y /boot/config-" + kernel));
}

// Adds or removes exclusion to the exclusion string
void Settings::addRemoveExclusion(bool add, QString exclusion)
{
    if (exclusion.startsWith('/')) {
        exclusion.remove(0, 1); // Remove preceding slash
    }
    if (add) {
        session_excludes.append('"' + exclusion + "\" ");
    } else {
        session_excludes.remove('"' + exclusion + "\" ");
    }
}

bool Settings::checkSnapshotDir() const
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (!Cmd().runAsRoot("mkdir -p \"" + snapshot_dir + '"', false)) {
        qDebug() << QObject::tr("Could not create working directory. ") + snapshot_dir;
        return false;
    }
    Cmd().runAsRoot("chown $(logname): \"" + snapshot_dir + '"');
    return true;
}

bool Settings::checkTempDir()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    // Set workdir location if not defined in .conf file, doesn't exist, or not supported partition
    if (tempdir_parent.isEmpty() || !QFile::exists(tempdir_parent) || !isOnSupportedPart(tempdir_parent)) {
        tempdir_parent = snapshot_dir;
        if (!isOnSupportedPart(
                snapshot_dir)) { // If not saving snapshot on a supported partition, put working dir in /tmp or /home
            tempdir_parent = largerFreeSpace("/tmp", "/home");
        } else {
            tempdir_parent = largerFreeSpace("/tmp", "/home", snapshot_dir);
        }
    }
    if (tempdir_parent == "/home") { // Replace /home with user home path
        QString userName = QString::fromUtf8(qgetenv("SUDO_USER")).trimmed();
        if (userName.isEmpty()) {
            userName = QString::fromUtf8(qgetenv("LOGNAME")).trimmed();
        }
        tempdir_parent = "/home/" + userName;
    }
    tmpdir.reset(new QTemporaryDir(tempdir_parent + "/mx-snapshot-XXXXXXXX"));
    if (!tmpdir->isValid()) {
        qDebug() << QObject::tr("Could not create temp directory. ") + tmpdir.data()->path();
        return false;
    }
    work_dir = tmpdir.data()->path();
    free_space_work = getFreeSpace(work_dir);

    QDir().mkpath(work_dir + "/iso-template/antiX");
    qDebug() << "Work directory is placed in" << tempdir_parent;
    return true;
}

QString Settings::getEditor() const
{
    QString editor = gui_editor;
    QString desktop_file;

    // If specified editor doesn't exist get the default one
    if (editor.isEmpty() || QStandardPaths::findExecutable(editor, {path}).isEmpty()) {
        QString default_editor = Cmd().getOut("xdg-mime query default text/plain");
        // Find first app with .desktop name that matches default_editors
        desktop_file
            = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, default_editor, QStandardPaths::LocateFile);
        QFile file(desktop_file);
        if (file.open(QIODevice::ReadOnly)) {
            while (!file.atEnd()) {
                QString line = file.readLine();
                if (line.contains(QRegularExpression("^Exec="))) {
                    editor = line.remove(QRegularExpression("^Exec=|%u|%U|%f|%F|%c|%C|-b")).trimmed();
                    break;
                }
            }
            file.close();
        }
        if (editor.isEmpty()) { // Use nano as backup editor
            editor = "nano";
        }
    }

    const bool isRoot = getuid() == 0;
    const bool isEditorThatElevates
        = QRegularExpression(R"((kate|kwrite|featherpad|code|codium)$)").match(editor).hasMatch();
    const bool isCliEditor = QRegularExpression(R"(nano|vi|vim|nvim|micro|emacs)").match(editor).hasMatch();

    QString elevate {QFile::exists("/usr/bin/pkexec") ? "/usr/bin/pkexec" : "/usr/bin/gksu"};
    if (isEditorThatElevates && !isRoot) {
        return editor;
    } else if (isRoot && isEditorThatElevates) {
        elevate += " --user $(logname)";
    }
    if (isCliEditor) {
        return "x-terminal-emulator -e " + elevate + " " + editor;
    }
    return elevate + " env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY " + editor;
}

// Return the size of the snapshot folder in MiB
QString Settings::getSnapshotSize() const
{
    qint64 totalSize = 0;
    if (QFileInfo::exists(snapshot_dir)) {
        QDir directory(snapshot_dir);
        QStringList isoFiles = directory.entryList(QStringList() << "*.iso", QDir::Files);
        for (const QString &file : isoFiles) {
            totalSize += QFileInfo(directory.absoluteFilePath(file)).size();
        }
    }
    return QString::number(totalSize / (1024 * 1024)) + "MiB";
}

// Number of snapshots in snapshot_dir
int Settings::getSnapshotCount() const
{
    if (QFileInfo::exists(snapshot_dir)) {
        QFileInfoList list = QDir(snapshot_dir).entryInfoList(QStringList("*.iso"), QDir::Files);
        return list.size();
    }
    return 0;
}

// Return KiB available space on the device
quint64 Settings::getFreeSpace(const QString &path)
{
    QStorageInfo storage(path + "/");
    if (!storage.isReady()) {
        qDebug() << "Cannot determine free space for" << path << ": Drive not ready or does not exist.";
        return 0;
    }
    if (storage.isReadOnly()) {
        qDebug() << "Cannot determine free space for" << path << ": Drive is read-only.";
        return 0;
    }
    return storage.bytesAvailable() / 1024;
}

// Return the XDG User Directory for each user with different localizations than English
QString Settings::getXdgUserDirs(const QString &folder)
{
    QStringList resultParts;
    resultParts.reserve(18); // For 3 users x 6 folders, not worth getting the number of users on the system

    const static QHash<QString, QString> englishDirs {
        {"DOCUMENTS", "Documents"}, {"DOWNLOAD", "Downloads"}, {"DESKTOP", "Desktop"},
        {"MUSIC", "Music"},         {"PICTURES", "Pictures"},  {"VIDEOS", "Videos"},
    };

    for (const QString &user : qAsConst(users)) {
        QString dir = Cmd().getOutAsRoot("runuser " + user + " -c \"xdg-user-dir " + folder + '"');

        // Skip if English name or of return folder is the home folder (if XDG-USER-DIR not defined)
        if (!dir.isEmpty() && englishDirs.value(folder) != dir.section('/', -1) && dir != "/home/" + user
            && dir != "/home/" + user + '/') {

            if (dir.startsWith('/')) {
                dir.remove(0, 1); // Remove trailing slash
            }

            QString exclusion = folder == "DESKTOP" ? "/!(minstall.desktop)" : "/*\" \"" + dir + "/.*";
            dir.append(exclusion);

            resultParts << dir;
        }
    }
    QString result = resultParts.join("\" \"");
    if (result.isEmpty()) {
        return {};
    }
    return "\" \"" + result;
}

void Settings::selectKernel()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    kernel.remove(QRegularExpression("^/boot/vmlinuz-")); // Remove path and part of name if passed as arg
    if (kernel.isEmpty()
        || !QFileInfo::exists("/boot/vmlinuz-" + kernel)) { // If kernel version not passed as arg, or incorrect
        kernel = current_kernel;
        if (!QFileInfo::exists(
                "/boot/vmlinuz-"
                + kernel)) { // If current kernel doesn't exist for some reason (e.g. WSL) in /boot pick latest kernel
            QDir directory("/boot");
            QStringList vmlinuzFiles = directory.entryList(QStringList() << "vmlinuz-*", QDir::Files, QDir::Name);
            if (!vmlinuzFiles.isEmpty()) {
                kernel = vmlinuzFiles.last().remove(QRegularExpression("^vmlinuz-"));
            }
            if (!QFileInfo::exists("/boot/vmlinuz-" + kernel)) {
                QString message = QObject::tr("Could not find a usable kernel");
                if (qApp->metaObject()->className() != QLatin1String("QApplication")) {
                    qDebug().noquote() << message;
                }
#ifndef CLI_BUILD
                else {
                    QMessageBox::critical(nullptr, QObject::tr("Error"), message);
                }
#endif
                exit(EXIT_FAILURE);
            }
        }
    }
    // Check if SQUASHFS is available
    if (QProcess::execute("grep", {"-q", "^CONFIG_SQUASHFS=[ym]", "/boot/config-" + kernel}) != 0) {
        QString message = QObject::tr("Current kernel doesn't support Squashfs, cannot continue.");
        if (qApp->metaObject()->className() != QLatin1String("QApplication")) {
            qDebug().noquote() << message;
        }
#ifndef CLI_BUILD
        else {
            QMessageBox::critical(nullptr, QObject::tr("Error"), message);
        }
#endif
        exit(EXIT_FAILURE);
    }
}

void Settings::setVariables()
{
    live = isLive();
    users = listUsers();
    x86 = isi386();

    QString distro_version_file;
    if (QFileInfo::exists("/etc/mx-version")) {
        distro_version_file = "/etc/mx-version";
    } else if (QFileInfo::exists("/etc/antix-version")) {
        distro_version_file = "/etc/antix-version";
    }

    if (QFileInfo::exists("/etc/lsb-release")) {
        project_name = Cmd().getOut("grep -oP '(?<=DISTRIB_ID=).*' /etc/lsb-release");
    } else {
        project_name = Cmd().getOut("lsb_release -i | cut -f2");
    }
    project_name.replace('"', "");
    if (!distro_version_file.isEmpty()) {
        distro_version = Cmd().getOut("cut -f1 -d'_' " + distro_version_file);
        distro_version.remove(QRegularExpression("^" + project_name + "_|^" + project_name + "-"));
    } else {
        distro_version = Cmd().getOut("lsb_release -r | cut -f2");
    }
    full_distro_name = project_name + "-" + distro_version + "_" + QString(x86 ? "386" : "x64");
    release_date = QDate::currentDate().toString("MMMM dd, yyyy");
    if (QFileInfo::exists("/etc/lsb-release")) {
        codename = Cmd().getOut("grep -oP '(?<=DISTRIB_CODENAME=).*' /etc/lsb-release");
    } else {
        codename = Cmd().getOut("lsb_release -c | cut -f2");
    }
    codename.replace('"', "");
    boot_options = monthly ? "quiet splash nosplash" : readKernelOpts();
}

QString Settings::getFilename() const
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (stamp == "datetime") {
        return snapshot_basename + "-" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmm") + ".iso";
    } else {
        QString name;
        QDir dir;
        int n = 1;
        do {
            name = snapshot_basename + QString::number(n) + ".iso";
            dir.setPath('"' + snapshot_dir + '/' + name + '"');
            n++;
        } while (QFileInfo::exists(dir.absolutePath()));
        return name;
    }
}

quint64 Settings::getLiveRootSpace()
{
    // rootspaceneeded is the size of the linuxfs file * a compression factor + contents of the rootfs, conservative
    // but fast factors are same as used in live-remaster

    // Load some live variables
    QSettings livesettings("/live/config/initrd.out", QSettings::NativeFormat);
    QString sqfile_full = livesettings.value("SQFILE_FULL", "/live/boot-dev/antiX/linuxfs").toString();
    QString toram_mp = livesettings.value("TORAM_MP", "/live/to-ram").toString();
    QString sqfile_path = livesettings.value("SQFILE_PATH", "antiX").toString().remove(QRegularExpression("^/+"));
    QString sqfile_name = livesettings.value("SQFILE_NAME", "linuxfs").toString();
    if (!toram_mp.isEmpty() && QFileInfo::exists(toram_mp + "/" + sqfile_path  + "/" + sqfile_name)) {
        sqfile_full = toram_mp + "/" + sqfile_path  + "/" + sqfile_name;
    }

    // Get compression factor by reading the linuxfs squasfs file, if available
    QString linuxfs_compression_type
        = Cmd().getOut("dd if=" + sqfile_full + " bs=1 skip=20 count=2 status=none 2>/dev/null |od -An -tdI");
    constexpr quint8 default_factor = 30;
    quint8 c_factor = default_factor;
    // gzip, xz, or lz4
    QMap<QString, QString> compression_types
        = {{"1", "gzip"}, {"2", "lzo"}, {"3", "lzma"}, {"4", "xz"}, {"5", "lz4"}, {"6", "zstd"}};
    if (compression_types.contains(linuxfs_compression_type)) {
        c_factor = compression_factor.value(compression_types.value(linuxfs_compression_type));
    } else {
        qWarning() << "Unknown compression type:" << linuxfs_compression_type;
    }
    quint64 rootfs_file_size = 0;
    quint64 linuxfs_file_size = QStorageInfo("/live/linux/").bytesTotal() * 100 / c_factor;
    if (QFileInfo::exists("/live/persist-root")) {
        rootfs_file_size = QStorageInfo("/live/persist-root/").bytesTotal();
    }

    // Add rootfs file size to the calculated linuxfs file size.  probaby conservative, as rootfs will likely have some
    // overlap with linuxfs
    return linuxfs_file_size + rootfs_file_size;
}

QString Settings::getUsedSpace()
{
    constexpr double factor = 1024 * 1024 * 1024;
    QString out = "\n- " + QObject::tr("Used space on / (root): ");
    if (live) {
        root_size = getLiveRootSpace();
        out += QString::number(static_cast<double>(root_size) / factor, 'f', 2) + "GiB -- " + QObject::tr("estimated");
    } else {
        QStorageInfo rootInfo("/");
        root_size = rootInfo.bytesTotal() - rootInfo.bytesFree();
        out += QString::number(static_cast<double>(root_size) / factor, 'f', 2) + "GiB";
    }
    QStorageInfo homeInfo("/home/");
    if (homeInfo.isValid() && homeInfo.isRoot()) {
        home_size = homeInfo.bytesTotal() - homeInfo.bytesFree();
        out.append("\n- " + QObject::tr("Used space on /home: ")
                   + QString::number(static_cast<double>(home_size) / factor, 'f', 2) + "GiB");
    } else {
        home_size = 0; // /home on root
    }
    return out;
}

// Check if running from a 32bit environment
bool Settings::isi386()
{
    return (QSysInfo::currentCpuArchitecture() == "i386");
}

int Settings::getDebianVerNum()
{
    QFile file("/etc/debian_version");
    QStringList list;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString line = in.readLine();
        list = line.split('.');
        file.close();
    } else {
        qCritical() << "Could not open /etc/debian_version:" << file.errorString() << "Assumes Bullseye";
        return Release::Bullseye;
    }
    bool ok = false;
    int ver = list.at(0).toInt(&ok);
    if (ok) {
        return ver;
    } else {
        QString verName = list.at(0).split('/').at(0);
        if (verName == "bullseye") {
            return Release::Bullseye;
        } else if (verName == "bookworm") {
            return Release::Bookworm;
        } else {
            qCritical() << "Unknown Debian version:" << ver << "Assumes Bullseye";
            return Release::Bullseye;
        }
    }
}

// Check if running from a live envoronment
bool Settings::isLive()
{
    return (QProcess::execute("mountpoint", {"-q", "/live/aufs"}) == 0);
}

// Check if the directory is on a Linux partition
bool Settings::isOnSupportedPart(const QString &dir)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    // Supported partition types (NTFS returns fuseblk)
    QStringList supported_partitions {"ext3", "ext4", "btrfs", "jfs", "xfs", "overlay", "fuseblk", "ramfs", "tmpfs"};
    QString part_type = QStorageInfo(dir + "/").fileSystemType();
    qDebug() << "detected partition" << part_type << "supported part:" << supported_partitions.contains(part_type);
    return supported_partitions.contains(part_type);
}

// Return the directory that has more free space available
QString Settings::largerFreeSpace(const QString &dir1, const QString &dir2)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (QStorageInfo(dir1 + "/").device() == QStorageInfo(dir2 + "/").device()) {
        return dir1;
    }
    quint64 dir1_free = getFreeSpace(dir1);
    quint64 dir2_free = getFreeSpace(dir2);
    return dir1_free >= dir2_free ? dir1 : dir2;
}

// Return the directory that has more free space available
QString Settings::largerFreeSpace(const QString &dir1, const QString &dir2, const QString &dir3)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    return largerFreeSpace(largerFreeSpace(dir1, dir2), dir3);
}

QString Settings::getFreeSpaceStrings(const QString &path)
{
    constexpr float factor = 1024 * 1024;
    free_space = getFreeSpace(path);
    QString out = QString::number(static_cast<double>(free_space) / factor, 'f', 2) + "GiB";

    qDebug().noquote() << QString("- " + QObject::tr("Free space on %1, where snapshot folder is placed: ").arg(path)
                                  + out)
                       << '\n';

    qDebug().noquote() << QObject::tr(
                              "The free space should be sufficient to hold the compressed data from / and /home\n\n"
                              "      If necessary, you can create more available space\n"
                              "      by removing previous snapshots and saved copies:\n"
                              "      %1 snapshots are taking up %2 of disk space.\n")
                              .arg(QString::number(getSnapshotCount()), getSnapshotSize());
    return out;
}

// Return a list of users that have folders in /home
QStringList Settings::listUsers()
{
    return Cmd().getOut("lslogins --noheadings -u -o user |grep -vw root", true).split('\n');
}

void Settings::excludeItem(const QString &item)
{
    static const QMap<QString, void (Settings::*)(bool)> itemExclusions {
        {QObject::tr("Desktop"), &Settings::excludeDesktop},
        {QObject::tr("Documents"), &Settings::excludeDocuments},
        {QObject::tr("Downloads"), &Settings::excludeDownloads},
        {QObject::tr("Music"), &Settings::excludeMusic},
        {QObject::tr("Networks"), &Settings::excludeNetworks},
        {QObject::tr("Pictures"), &Settings::excludePictures},
        {"Steam", &Settings::excludeSteam},
        {QObject::tr("Videos"), &Settings::excludeVideos},
        {"VirtualBox", &Settings::excludeVirtualBox}};

    auto it = itemExclusions.find(item);
    if (it != itemExclusions.end()) {
        (this->*(it.value()))(true);
    }
}

void Settings::excludeDesktop(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(Exclude::Desktop);
    }
    QString exclusion = "/home/*/Desktop/!(minstall.desktop)" + getXdgUserDirs("DESKTOP");
    addRemoveExclusion(exclude, exclusion);
}

void Settings::excludeDocuments(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(Exclude::Documents);
    }
    QString folder {"home/*/Documents/"};
    QString xdg_name {"DOCUMENTS"};
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(exclude, exclusion);
}

void Settings::excludeDownloads(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(Exclude::Downloads);
    }
    QString folder {"home/*/Downloads/"};
    QString xdg_name {"DOWNLOAD"};
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(exclude, exclusion);
}

void Settings::excludeMusic(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(Exclude::Music);
    }
    QString folder {"home/*/Music/"};
    QString xdg_name {"MUSIC"};
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(exclude, exclusion);
}

void Settings::excludeNetworks(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(Exclude::Networks);
    }
    addRemoveExclusion(exclude, QStringLiteral("/etc/NetworkManager/system-connections/*"));
    addRemoveExclusion(exclude, QStringLiteral("/etc/wicd/*"));
    addRemoveExclusion(exclude, QStringLiteral("/var/lib/connman/*"));
}

void Settings::excludePictures(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(Exclude::Pictures);
    }
    QString folder {"home/*/Pictures/"};
    QString xdg_name {"PICTURES"};
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(exclude, exclusion);
}

void Settings::excludeSteam(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(Exclude::Steam);
    }
    addRemoveExclusion(exclude, QStringLiteral("home/*/.steam"));
    addRemoveExclusion(exclude, QStringLiteral("home/*/.local/share/Steam"));
}

void Settings::excludeSwapFile()
{
    QFile file("/etc/fstab");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open /etc/fstab";
        return;
    }
    QByteArray content = file.readAll();
    QStringList lines = QString::fromUtf8(content).split('\n');

    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.startsWith('/') && !trimmedLine.startsWith("/dev/")) {
            QStringList parts = trimmedLine.split(QRegularExpression("\\s+"));
            if (parts.size() > 3 && parts.at(2) == "swap") {
                addRemoveExclusion(true, parts[0].remove(0, 1));
            }
        }
    }
}

void Settings::excludeVideos(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(Exclude::Videos);
    }
    QString folder {"home/*/Videos/"};
    QString xdg_name {"VIDEOS"};
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(exclude, exclusion);
}

void Settings::excludeVirtualBox(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) {
        exclusions.setFlag(Exclude::VirtualBox);
    }
    addRemoveExclusion(exclude, QStringLiteral("home/*/VirtualBox VMs"));
}

// Load settings from config file
void Settings::loadConfig()
{
    QSettings settingsSystem(config_file.fileName(), QSettings::IniFormat);
    QSettings settingsUser;

    // Read all keys from system settings
    settingsSystem.beginGroup("");
    QStringList systemKeys = settingsSystem.allKeys();
    settingsSystem.endGroup();

    // Merge system settings into user settings
    foreach (const QString &key, systemKeys) {
        if (!settingsUser.contains(key)) {
            QVariant value = settingsSystem.value(key);
            settingsUser.setValue(key, value);
        }
    }

    session_excludes.clear();
    snapshot_dir = settingsUser.value("snapshot_dir", "/home/snapshot").toString();
    if (!snapshot_dir.endsWith("/snapshot")) {
        snapshot_dir = QDir::cleanPath(snapshot_dir + "/snapshot");
    }
    snapshot_excludes.setFileName(
        settingsUser
            .value("snapshot_excludes",
                   QDir::cleanPath("/usr/local/share/excludes/" + qApp->applicationName() + "-exclude.list"))
            .toString());
    snapshot_basename = settingsUser.value("snapshot_basename", "snapshot").toString();
    make_md5sum = settingsUser.value("make_md5sum", "no").toString() != "no";
    make_sha512sum = settingsUser.value("make_sha512sum", "no").toString() != "no";
    make_isohybrid = settingsUser.value("make_isohybrid", "yes").toString() == "yes";
    compression = settingsUser.value("compression", "zstd").toString();
    mksq_opt = settingsUser.value("mksq_opt").toString();
    edit_boot_menu = settingsUser.value("edit_boot_menu", "no").toString() != "no";
    gui_editor = settingsUser.value("gui_editor").toString();
    stamp = settingsUser.value("stamp").toString();
    force_installer = settingsUser.value("force_installer", true).toBool();
    tempdir_parent = settingsUser.value("workdir").toString();
    cores = settingsUser.value("cores", max_cores).toUInt();
    throttle = settingsUser.value("throttle", 0).toUInt();
    reset_accounts = false;
}

void Settings::excludeAll()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    excludeDesktop(true);
    excludeDocuments(true);
    excludeDownloads(true);
    excludeMusic(true);
    excludeNetworks(true);
    excludePictures(true);
    excludeSteam(true);
    excludeVideos(true);
    excludeVirtualBox(true);
}

void Settings::otherExclusions()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    // Add exclusions snapshot and work dirs
    addRemoveExclusion(true, snapshot_dir);
    addRemoveExclusion(true, work_dir);

    if (reset_accounts) {
        addRemoveExclusion(true, QStringLiteral("/etc/minstall.conf"));
        // Exclude /etc/localtime if link and timezone not America/New_York
        QFileInfo localtimeInfo("/etc/localtime");
        QFile timezoneFile("/etc/timezone");
        if (localtimeInfo.isSymLink()) {
            if (timezoneFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&timezoneFile);
                QString timezone = in.readLine();
                if (timezone != "America/New_York") {
                    addRemoveExclusion(true, "/etc/localtime");
                }
                timezoneFile.close();
            }
        }
    }
    excludeSwapFile();
}

void Settings::processArgs(const QCommandLineParser &arg_parser)
{
    shutdown = arg_parser.isSet("shutdown");
    kernel = arg_parser.value("kernel");
    preempt = arg_parser.isSet("preempt");
    QDir dir;
    if (!arg_parser.value("directory").isEmpty() && QFileInfo::exists(arg_parser.value("directory"))) {
        dir.setPath(arg_parser.value("directory"));
        snapshot_dir = dir.absolutePath() + "/snapshot";
    }

    if (!arg_parser.value("workdir").isEmpty() && QFileInfo::exists(arg_parser.value("workdir"))) {
        dir.setPath(arg_parser.value("workdir"));
        tempdir_parent = dir.absolutePath();
    }

    if (!arg_parser.value(QStringLiteral("file")).isEmpty()) {
        snapshot_name = arg_parser.value("file") + (arg_parser.value("file").endsWith(".iso") ? QString() : ".iso");
    } else {
        snapshot_name = getFilename();
    }
    if (QFile::exists(snapshot_dir + '/' + snapshot_name)) {
        QString message
            = QObject::tr("Output file %1 already exists. Please use another file name, or delete the existent file.")
                  .arg(snapshot_dir + '/' + snapshot_name);
        if (qApp->metaObject()->className() != QLatin1String("QApplication")) {
            qDebug().noquote() << message;
        }
#ifndef CLI_BUILD
        else {
            QMessageBox::critical(nullptr, QObject::tr("Error"), message);
        }
#endif
        exit(EXIT_FAILURE);
    }
    reset_accounts = arg_parser.isSet("reset");
    if (reset_accounts) {
        excludeAll();
    }
    if (arg_parser.isSet("month")) {
        reset_accounts = true;
    }
    if (arg_parser.isSet("checksums")) {
        make_sha512sum = make_md5sum = true;
    }
    if (arg_parser.isSet("month")) {
        make_sha512sum = true;
        make_md5sum = false;
    }
    if (arg_parser.isSet("no-checksums")) {
        make_sha512sum = make_md5sum = false;
    }
    if (!arg_parser.value("compression").isEmpty()) {
        compression = arg_parser.value("compression");
    }
    if (!arg_parser.value("compression-level").isEmpty()) {
        mksq_opt = arg_parser.value("compression-level");
    }
    if (!arg_parser.value("cores").isEmpty()) {
        bool ok {false};
        uint val = arg_parser.value("cores").toUInt(&ok);
        if (!ok || val == 0 || val > max_cores) {
            qDebug() << "Invalid number of cores argument, will use the default:" << cores;
        } else {
            cores = val;
        }
    }
    if (!arg_parser.value("throttle").isEmpty()) {
        bool ok {false};
        uint val = arg_parser.value("throttle").toUInt(&ok);
        if (!ok || val > 99) {
            qDebug() << "Invalid argument for throttle, will use the default:" << throttle;
        } else {
            throttle = val;
        }
    }
    selectKernel();
}

void Settings::processExclArgs(const QCommandLineParser &arg_parser)
{
    static const QSet<QString> valid_options {"Desktop",  "Documents", "Downloads", "Music",     "Networks",
                                              "Pictures", "Steam",     "Videos",    "VirtualBox"};
    if (arg_parser.isSet("exclude")) {
        QStringList options = arg_parser.values("exclude");
        for (const QString &option : options) {
            if (valid_options.contains(option)) {
                excludeItem(option);
            } else {
                qDebug() << "Unknown option:" << option << '\n'
                         << "Please use one of these options" << valid_options.values();
            }
        }
    }
}

// Use script to return useful kernel options
QString Settings::readKernelOpts()
{
    return Cmd().getOut((QString("/usr/share/%1/scripts/snapshot-bootparameter.sh | tr '\n' ' '")
                             .arg(QCoreApplication::applicationName())));
}

void Settings::setMonthlySnapshot(const QCommandLineParser &arg_parser)
{
    QString name;
    if (QFileInfo::exists(QStringLiteral("/etc/mx-version"))) {
        name = Cmd().getOut("cat /etc/mx-version |cut -f1 -d' '");
    } else {
        qDebug() << "/etc/mx-version not found. Not MX Linux?";
        name = "MX_" + QString(x86 ? "386" : "x64");
    }
    if (arg_parser.value("file").isEmpty()) {
        auto month = QDate::currentDate().toString("MMMM");
        auto suffix = name.section('_', 1, 1);
        if (qgetenv("DESKTOP_SESSION") == "plasma") {
            suffix = "KDE";
        }
        snapshot_name = name.section('_', 0, 0) + '_' + month + '_' + suffix + ".iso";
    }
    if (QFile::exists(snapshot_dir + '/' + snapshot_name)) {
        QString message
            = QObject::tr("Output file %1 already exists. Please use another file name, or delete the existent file.")
                  .arg(snapshot_dir + '/' + snapshot_name);
        if (qApp->metaObject()->className() != QLatin1String("QApplication")) {
            qDebug().noquote() << message;
        }
#ifndef CLI_BUILD
        else {
            QMessageBox::critical(nullptr, QObject::tr("Error"), message);
        }
#endif
        exit(EXIT_FAILURE);
    }
    if (arg_parser.value("compression").isEmpty()) {
        compression = "zstd";
    }
    reset_accounts = true;
    boot_options.remove("toram");
    excludeAll();
}
