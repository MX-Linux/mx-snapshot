/**********************************************************************
 *  settings.cpp
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

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QSettings>


#include "settings.h"

Settings::Settings(const QCommandLineParser &arg_parser)
{
    if (QFileInfo::exists("/tmp/installed-to-live/cleanup.conf")) // cleanup installed-to-live from other sessions
        system("installed-to-live cleanup");
    shell = new Cmd;

    loadConfig();  // load settings from .conf file
    setVariables();
    processArgs(arg_parser);
    if (arg_parser.isSet("month"))
        setMonthlySnapshot(arg_parser);
    processExclArgs(arg_parser);
}

Settings::~Settings()
{
}

// check if compression is available in the kernel (lz4, lzo, xz)
bool Settings::checkCompression()
{
    if (QFileInfo::exists("/boot/config-" + kernel)) // return true if cannot check config file
        return true;

    if (compression == "lz4")
        return (shell->run("grep ^CONFIG_SQUASHFS_LZ4=y /boot/config-" + kernel.toUtf8()));
    else if (compression == "xz")
        return (shell->run("grep ^CONFIG_SQUASHFS_XZ=y /boot/config-" + kernel.toUtf8()));
    else if (compression == "lzo")
        return (shell->run("grep ^CONFIG_SQUASHFS_LZO=y /boot/config-" + kernel.toUtf8()));
    return true;
}

// adds or removes exclusion to the exclusion string
void Settings::addRemoveExclusion(bool add, QString exclusion)
{
    if (exclusion.startsWith("/"))
        exclusion.remove(0, 1); // remove preceding slash
    if (add) {
        if (session_excludes.isEmpty())
            session_excludes.append("-e \"" + exclusion + "\"");
        else
            session_excludes.append(" \"" + exclusion + "\"");
    } else {
        session_excludes.remove(" \"" + exclusion + "\"");
        if (session_excludes == "-e")
            session_excludes = "";
    }
}

bool Settings::checkSnapshotDir()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (!QDir().mkpath(snapshot_dir)) {
        qDebug() << QObject::tr("Could not create working directory. ") + snapshot_dir;
        return false;
    }
    system("chown $(logname):$(logname) \"/" + snapshot_dir.toUtf8() + "\"");
    // Create a work_dir
    tempdir_parent = snapshot_dir;
    if (!isOnSupportedPart(snapshot_dir)) // if not saving snapshot on a Linux partition put working dir in /tmp or /home
        tempdir_parent = largerFreeSpace("/tmp", "/home");
    else
        tempdir_parent = largerFreeSpace("/tmp", "/home", snapshot_dir);
    return true;
}

bool Settings::checkTempDir()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    tmpdir.reset(new QTemporaryDir(tempdir_parent + "/mx-snapshot-XXXXXXXX"));
    if(!tmpdir->isValid()) {
        qDebug() << QObject::tr("Could not create temp directory. ") + tmpdir.data()->path();
        return false;
    }
    work_dir = tmpdir.data()->path();
    free_space_work = getFreeSpace(work_dir);

    QDir().mkpath(work_dir + "/iso-template/antiX");
    return true;
}

short Settings::getDebianVersion()
{
    return shell->getCmdOut("cat /etc/debian_version |cut -f1 -d'.'", true).toShort();
}

QString Settings::getEditor()
{
    QString editor;
    if (system("command -v " + gui_editor.fileName().toUtf8()) != 0) {  // if specified editor doesn't exist get the default one
        QString local = QFile::exists(QDir::homePath() + "/.local/share/applications") ? "/.local/share/applications " : " ";
        QString desktop_file = shell->getCmdOut("find " + local + "/usr/share/applications -name $(xdg-mime query default text/plain) | grep -m1 .");
        editor = shell->getCmdOut("grep -m1 ^Exec " + desktop_file + " |cut -d= -f2 |cut -d\" \" -f1", true);
        if (editor.isEmpty() || system("command -v " + editor.toUtf8()) != 0) { // if default one doesn't exit use nano as backup editor
            editor = "x-terminal-emulator -e nano";
        } else if (editor == "kate" || editor == "kwrite") { // need to run these as normal user
            editor = "runuser -u $(logname) " + editor;
        }
    } else {
        editor = gui_editor.fileName();
    }
    return editor;
}

// return the size of the snapshot folder
QString Settings::getSnapshotSize()
{
    QString size;
    if (QFileInfo::exists(snapshot_dir)) {
        QString cmd = QString("find \"%1\" -maxdepth 1 -type f -name '*.iso' -exec du -shc {} + |tail -1 |awk '{print $1}'").arg(snapshot_dir);
        size = shell->getCmdOut(cmd);
        if (!size.isEmpty())
            return size;
    }
    return "0";
}

// return number of snapshots in snapshot_dir
int Settings::getSnapshotCount()
{
    if (QFileInfo::exists(snapshot_dir)) {
        QFileInfoList list = QDir(snapshot_dir).entryInfoList(QStringList("*.iso"), QDir::Files);
        return list.size();
    }
    return 0;
}

quint64 Settings::getFreeSpace(const QString &path)
{
    bool ok;
    quint64 result = shell->getCmdOut(QString("df -k --output=avail \"%1\" |tail -n1").arg(path)).toULongLong(&ok);
    if (not ok) {
        qDebug() << "Can't calculate free space on" << path;
        return 0;
    }
    return result;
}

// return the XDG User Directory for each user with different localizations than English
QString Settings::getXdgUserDirs(const QString& folder)
{
    QString result = "";

    for (const QString &user : users) {
        QString dir;
        bool success = shell->run("runuser -l " + user + " -c \"xdg-user-dir " + folder + "\"", dir);
        if (success) {
            if (englishDirs.value(folder) == dir.section("/", -1) || dir.trimmed() == "/home/" + user || dir.trimmed() == "/home/" + user + "/" || dir.isEmpty()) // skip if English name or of return folder is the home folder (if XDG-USER-DIR not defined)
                continue;
            if (dir.startsWith("/"))
                dir.remove(0, 1); // remove training slash
            (folder == "DESKTOP") ? dir.append("/!(minstall.desktop)") : dir.append("/*\" \"" + dir + "/.*");
            (result.isEmpty()) ? result.append("\" \"" + dir) : result.append(" \"" + dir);
            result.append("\""); // close the quote for each user, will strip the last one before returning;
        }
    }
    result.chop(1); // chop the last quote, will be added later on in addRemoveExclusion
    return result;
}

void Settings::selectKernel()
{
    if (kernel.startsWith("/boot/vmlinuz-")) kernel.remove("/boot/vmlinuz-"); // remove path and part of name if passed as arg
    if (kernel.isEmpty() || !QFileInfo::exists("/boot/vmlinuz-" + kernel)) {  // if kernel version not passed as arg, or incorrect
        kernel = shell->getCmdOut("uname -r");
        if (!QFileInfo::exists("/boot/vmlinuz-" + kernel)) { // if current kernel doesn't exist for some reason (e.g. WSL) in /boot pick first kernel
             kernel = shell->getCmdOut("ls -1 /boot/vmlinuz* | sort | tail -n1").remove("/boot/vmlinuz-");
             if (!QFileInfo::exists("/boot/vmlinuz-" + kernel)) {
                 qDebug() << QObject::tr("Could not find a usable kernel");
                 exit(EXIT_FAILURE);
             }
        }
    }
    // Check if SQUASHFS is available
    if (system("grep -q ^CONFIG_SQUASHFS=[ym] /boot/config-" + kernel.toUtf8()) != 0) {
        qDebug() << QObject::tr("Current kernel doesn't support Squashfs, cannot continue.");
        exit(EXIT_FAILURE);
    }
}

void Settings::setVariables()
{
    englishDirs = {
        {"DOCUMENTS", "Documents"},
        {"DOWNLOAD", "Downloads"},
        {"DESKTOP", "Desktop"},
        {"MUSIC", "Music"},
        {"PICTURES", "Pictures"},
        {"VIDEOS", "Videos"},
    };

    live = isLive();
    users = listUsers();
    i686 = isi686();
    debian_version = getDebianVersion();
}

// Create the output filename
QString Settings::getFilename()
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
            dir.setPath("\"" + snapshot_dir + "/" + name + "\"");
            n++;
        } while (QFileInfo::exists(dir.absolutePath()));
        return name;
    }
}


quint64 Settings::getLiveRootSpace()
{
    // rootspaceneeded is the size of the linuxfs file * a compression factor + contents of the rootfs.  conservative but fast
    // factors are same as used in live-remaster

    // load some live variables
    QSettings livesettings("/live/config/initrd.out", QSettings::NativeFormat);
    QString sqfile_full = livesettings.value("SQFILE_FULL", "/live/boot-dev/antiX/linuxfs").toString();

    // get compression factor by reading the linuxfs squasfs file, if available
    QString linuxfs_compression_type = shell->getCmdOut("dd if=" + sqfile_full + " bs=1 skip=20 count=2 status=none 2>/dev/null |od -An -tdI");
    uint c_factor;
    // gzip, xz, or lz4
    if (linuxfs_compression_type == "1") c_factor = compression_factor.value("gzip");
    else if (linuxfs_compression_type == "2") c_factor = compression_factor.value("lzo"); // lzo, not used by antiX
    else if (linuxfs_compression_type == "3") c_factor = compression_factor.value("lzma"); // lzma, not used by antiX
    else if (linuxfs_compression_type == "4") c_factor = compression_factor.value("xz");
    else if (linuxfs_compression_type == "5") c_factor = compression_factor.value("lz4");
    else c_factor = 30; //anything else or linuxfs not reachable (toram), should be pretty conservative

    quint64 rootfs_file_size = 0;
    quint64 linuxfs_file_size = shell->getCmdOut("df -k /live/linux --output=used --total |tail -n1").toULongLong() * 100 / c_factor;
    if (QFileInfo::exists("/live/persist-root"))
        rootfs_file_size = shell->getCmdOut("df -k /live/persist-root --output=used --total |tail -n1").toULongLong();

    // add rootfs file size to the calculated linuxfs file size.  probaby conservative, as rootfs will likely have some overlap with linuxfs
    return linuxfs_file_size + rootfs_file_size;
}

QString Settings::getUsedSpace()
{
    bool ok;
    QString out = "\n- " + QObject::tr("Used space on / (root): ");
    if (live) {
        root_size = getLiveRootSpace();
        out += QString::number(root_size / 1048576.0, 'f', 2) + "GiB" + " -- " + QObject::tr("estimated");
    } else {
        root_size = shell->getCmdOut("df -k --output=used / |tail -n1").toULongLong(&ok);
        if (not ok) return "Can't calculate free space on root";
        out += QString::number(root_size / 1048576.0, 'f', 2) + "GiB";
    }
    if (shell->run("mountpoint -q /home")) {
        home_size = shell->getCmdOut("df -k --output=used /home |tail -n1").toULongLong(&ok);
        if (not ok) return "Can't calculate free space on /home";
        out.append("\n- " + QObject::tr("Used space on /home: ") + QString::number(home_size / 1048576.0, 'f', 2) + "GiB");
    } else {
        home_size = 0; // /home on root
    }
    return out;
}


// Check if running from a 32bit environment
bool Settings::isi686()
{
    return (shell->getCmdOut("uname -m", true) == "i686");
}

// Check if running from a live envoronment
bool Settings::isLive()
{
    return (shell->run("mountpoint -q /live/aufs", true));
}

// checks if the directory is on a Linux partition
bool Settings::isOnSupportedPart(const QString &dir)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QStringList supported_partitions = (QStringList() << "ext2/ext3" << "btrfs" << "jfs" << "reiserfs" << "xfs" << "fuseblk"); // supported partition types (NTFS returns fuseblk)
    QString part_type = shell->getCmdOut("stat --file-system --format=%T \"" + dir + "\"").trimmed();
    qDebug() << "detected partition" << part_type << "supported part:" << supported_partitions.contains(part_type);
    return supported_partitions.contains(part_type);
}

// return the directory that has more free space available
QString Settings::largerFreeSpace(const QString &dir1, const QString &dir2)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (shell->getCmdOut("stat -c '%d' " + dir1) == shell->getCmdOut("stat -c '%d' " + dir2)) return dir1;
    quint64 dir1_free = getFreeSpace(dir1);
    quint64 dir2_free = getFreeSpace(dir2);
    return dir1_free >= dir2_free ? dir1 : dir2;
}

// return the directory that has more free space available
QString Settings::largerFreeSpace(const QString &dir1, const QString &dir2, const QString &dir3)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    return largerFreeSpace(largerFreeSpace(dir1, dir2), dir3);
}

QString Settings::getFreeSpaceStrings(const QString &path)
{
    free_space = getFreeSpace(path);
    QString out = QString::number(free_space / 1048576.0, 'f', 2) + "GiB";

    qDebug().noquote() << QString("- " + QObject::tr("Free space on %1, where snapshot folder is placed: ").arg(path) + out) << endl;

    qDebug().noquote() << QObject::tr("The free space should be sufficient to hold the compressed data from / and /home\n\n"
                                       "      If necessary, you can create more available space\n"
                                       "      by removing previous snapshots and saved copies:\n"
                                       "      %1 snapshots are taking up %2 of disk space.\n").arg(QString::number(getSnapshotCount())).arg(getSnapshotSize());
    return out;
}

// return a list of users that have folders in /home
QStringList Settings::listUsers()
{
    return shell->getCmdOut("lslogins --noheadings -u -o user |grep -vw root", true).split("\n");
}

void Settings::excludeItem(const QString &item)
{
    if (item == QObject::tr("Desktop") or item == "Desktop") excludeDesktop(true);
    if (item == QObject::tr("Documents") or item == "Documents") excludeDocuments(true);
    if (item == QObject::tr("Downloads") or item == "Downloads") excludeDownloads(true);
    if (item == QObject::tr("Music") or item == "Music") excludeMusic(true);
    if (item == QObject::tr("Networks") or item == "Networks") excludeNetworks(true);
    if (item == QObject::tr("Pictures") or item == "Pictures") excludePictures(true);
    if (item == QObject::tr("Videos") or item == "Videos") excludeVideos(true);
}

void Settings::excludeDesktop(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) exclusions.setFlag(Exclude::Desktop);
    QString exclusion = "/home/*/Desktop/!(minstall.desktop)" + getXdgUserDirs("DESKTOP");
    addRemoveExclusion(exclude, exclusion);
}

void Settings::excludeDocuments(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) exclusions.setFlag(Exclude::Documents);
    QString folder = "home/*/Documents/";
    QString xdg_name = "DOCUMENTS";
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(exclude, exclusion);
}

void Settings::excludeDownloads(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) exclusions.setFlag(Exclude::Downloads);
    QString folder = "home/*/Downloads/";
    QString xdg_name = "DOWNLOAD";
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(exclude, exclusion);
}

void Settings::excludeMusic(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) exclusions.setFlag(Exclude::Music);
    QString folder = "home/*/Music/";
    QString xdg_name = "MUSIC";
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(exclude, exclusion);
}

void Settings::excludeNetworks(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) exclusions.setFlag(Exclude::Networks);
    // Network Manager
    addRemoveExclusion(exclude, "/etc/NetworkManager/system-connections/*");
    // WiCD
    addRemoveExclusion(exclude, "/etc/wicd/*");
    // connman
    addRemoveExclusion(exclude, "/var/lib/connman/*");
}

void Settings::excludePictures(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) exclusions.setFlag(Exclude::Pictures);
    QString folder = "home/*/Pictures/";
    QString xdg_name = "PICTURES";
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(exclude, exclusion);
}

void Settings::excludeVideos(bool exclude)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclude) exclusions.setFlag(Exclude::Videos);
    QString folder = "home/*/Videos/";
    QString xdg_name = "VIDEOS";
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(exclude, exclusion);
}

// load settings from config file
void Settings::loadConfig()
{
    config_file.setFileName("/etc/" + qApp->applicationName() + ".conf");
    QSettings settings(config_file.fileName(), QSettings::IniFormat);

    session_excludes = "";
    snapshot_dir = settings.value("snapshot_dir", "/home/snapshot").toString();
    if (not snapshot_dir.endsWith("/snapshot")) snapshot_dir += (snapshot_dir.endsWith("/") ? "snapshot" : "/snapshot");
    snapshot_excludes.setFileName(settings.value("snapshot_excludes", "/usr/local/share/excludes/mx-snapshot-exclude.list").toString());
    snapshot_basename = settings.value("snapshot_basename", "snapshot").toString();
    make_chksum = settings.value("make_md5sum", "no").toString() == "no" ? false : true;
    make_isohybrid = settings.value("make_isohybrid", "yes").toString() == "yes" ? true : false;
    compression = settings.value("compression", "lz4").toString();
    mksq_opt = settings.value("mksq_opt").toString();
    edit_boot_menu = settings.value("edit_boot_menu", "no").toString() == "no" ? false : true;
    gui_editor.setFileName(settings.value("gui_editor", "/usr/bin/featherpad").toString());
    stamp = settings.value("stamp").toString();
    force_installer = settings.value("force_installer", "true").toBool();
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
    excludeVideos(true);
}

void Settings::otherExclusions()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    // add exclusions snapshot and work dirs
    addRemoveExclusion(true, snapshot_dir);
    addRemoveExclusion(true, work_dir);

    if (reset_accounts) {
        addRemoveExclusion(true, "/etc/minstall.conf");
        // exclude /etc/localtime if link and timezone not America/New_York
        if (shell->run("test -L /etc/localtime") && shell->getCmdOut("cat /etc/timezone") != "America/New_York" )
            addRemoveExclusion(true, "/etc/localtime");
    }
}

void Settings::processArgs(const QCommandLineParser &arg_parser)
{
    kernel = arg_parser.value("kernel");
    preempt = arg_parser.isSet("preempt");
    if (!arg_parser.value("directory").isEmpty() && QFileInfo::exists(arg_parser.value("directory")))
        snapshot_dir = arg_parser.value("directory") + (snapshot_dir.endsWith("/") ? "snapshot" : "/snapshot");
    if (!arg_parser.value("file").isEmpty())
        snapshot_name = arg_parser.value("file") + (arg_parser.value("file").endsWith(".iso") ? QString() : ".iso");
    else
        snapshot_name = getFilename();
    reset_accounts = arg_parser.isSet("reset");
    if (reset_accounts) excludeAll();
    if (arg_parser.isSet("month")) reset_accounts = true;
    if (arg_parser.isSet("checksums") || arg_parser.isSet("month"))
        make_chksum = true;
    if (arg_parser.isSet("no-checksums"))
        make_chksum = false;
    if (!arg_parser.value("compression").isEmpty())
        compression = arg_parser.value("compression");
    selectKernel();
}

void Settings::processExclArgs(const QCommandLineParser &arg_parser)
{
    if(!arg_parser.values("exclude").isEmpty()) {
        QStringList options = arg_parser.values("exclude");
        QStringList valid_options {"Desktop", "Documents", "Downloads", "Music", "Networks", "Pictures", "Videos"};
        for (const QString &option : options )
            if (valid_options.contains(option))
                excludeItem(option);
    }
}

void Settings::setMonthlySnapshot(const QCommandLineParser &arg_parser)
{
    QString name;
    if (QFileInfo::exists("/etc/mx-version")) {
        name = shell->getCmdOut("cat /etc/mx-version | cut -f1 -d' '");
    } else {
        qDebug() << "/etc/mx-version not found. Not MX Linux?";
        name = "MX_" + QString(i686 ? "386" : "x64");
    }
    if (arg_parser.value("file").isEmpty())
        snapshot_name = name.section("_", 0, 0) + "_" + QDate::currentDate().toString("MMMM") + "_" + name.section("_", 1, 1) + ".iso";
    if (arg_parser.value("compression").isEmpty())
        compression = "xz";
    reset_accounts = true;
    excludeAll();
}
