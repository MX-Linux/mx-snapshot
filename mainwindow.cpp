/**********************************************************************
 *  mainwindow.cpp
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

#include "about.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QScrollBar>
#include <QTextStream>
#include <QKeyEvent>
#include <QTime>

#include <QDebug>

MainWindow::MainWindow(QWidget *parent, QStringList args) :
    QDialog(parent),
    ui(new Ui::MainWindow)
{
    qDebug().noquote() << QCoreApplication::applicationName() << "version:" << VERSION;
    ui->setupUi(this);
    setWindowFlags(Qt::Window); // for the close, min and max buttons
    shell = new Cmd(this);
    this->args = args;

    QFont font("monospace");
    font.setStyleHint(QFont::Monospace);
    ui->outputBox->setFont(font);

    connect(&timer, &QTimer::timeout, this, &MainWindow::progress);
    connect(shell, &Cmd::started, this, &MainWindow::procStart);
    connect(shell, &Cmd::finished, this, &MainWindow::procDone);
    connect(shell, &Cmd::outputAvailable, [](const QString &out) {qDebug() << out.trimmed();});
    connect(shell, &Cmd::errorAvailable, [](const QString &out) {qWarning() << out.trimmed();});

    this->setWindowTitle(tr("MX Snapshot"));
    ui->buttonBack->setHidden(true);
    ui->buttonSelectSnapshot->setHidden(true);
    ui->stackedWidget->setCurrentIndex(0);

    live = isLive();
    users = listUsers();
    i686 = isi686();
    debian_version = getDebianVersion();

    englishDirs = {
        {"DOCUMENTS", "Documents"},
        {"DOWNLOAD", "Downloads"},
        {"DESKTOP", "Desktop"},
        {"MUSIC", "Music"},
        {"PICTURES", "Pictures"},
        {"VIDEOS", "Videos"},
    };

    setup();
    reset_accounts = false;
    if (args.contains("--monthly") || args.contains("-m")) {
        QString name = shell->getCmdOut("cat /etc/mx-version | /usr/bin/cut -f1 -d' '");
        ui->lineEditName->setText(name.section("_", 0, 0) + "_" + QDate::currentDate().toString("MMMM") + "_" + name.section("_", 1, 1) + ".iso");
        ui->cbCompression->setCurrentIndex(ui->cbCompression->findText("xz")); // use XZ by default on Monthly snapshots
        ui->buttonNext->click();
        ui->radioRespin->click();
        ui->buttonNext->click();
    } else {
        listUsedSpace();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

// load settings or use the default value
void MainWindow::loadSettings()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    config_file.setFileName("/etc/mx-snapshot.conf");
    QSettings settings(config_file.fileName(), QSettings::IniFormat);

    session_excludes = "";
    snapshot_dir = settings.value("snapshot_dir", "/home/snapshot").toString();
    ui->labelSnapshotDir->setText(snapshot_dir.absolutePath());
    snapshot_excludes.setFileName(settings.value("snapshot_excludes", "/usr/local/share/excludes/mx-snapshot-exclude.list").toString());
    snapshot_basename = settings.value("snapshot_basename", "snapshot").toString();
    make_chksum = settings.value("make_md5sum", "no").toString();
    make_isohybrid = settings.value("make_isohybrid", "yes").toString();
    compression = settings.value("compression", "lz4").toString();
    mksq_opt = settings.value("mksq_opt").toString();
    edit_boot_menu = settings.value("edit_boot_menu", "no").toString();
    lib_mod_dir = settings.value("lib_mod_dir", "/lib/modules/").toString();
    gui_editor.setFileName(settings.value("gui_editor", "/usr/bin/featherpad").toString());
    stamp = settings.value("stamp").toString();
    force_installer = settings.value("force_installer", "true").toBool();
    ui->lineEditName->setText(getFilename());
}

// setup/refresh versious items first time program runs
void MainWindow::setup()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    qApp->processEvents();
    this->show();
    this->setWindowTitle(tr("MX Snapshot"));
    ui->buttonBack->setHidden(true);
    ui->buttonSelectSnapshot->setHidden(false);
    ui->stackedWidget->setCurrentIndex(0);
    ui->buttonCancel->setEnabled(true);
    ui->buttonNext->setEnabled(true);

    loadSettings();
    listFreeSpace();

    ui->cbCompression->setCurrentIndex(ui->cbCompression->findText(compression));
}


// Util function for replacing strings in files
bool MainWindow::replaceStringInFile(QString old_text, QString new_text, QString file_path)
{
    QString cmd = QString("sed -i 's/%1/%2/g' \"%3\"").arg(old_text).arg(new_text).arg(file_path);
    return shell->run(cmd);
}

// Check if running from a live envoronment
bool MainWindow::isLive()
{
    return (shell->run("mountpoint -q /live/aufs"));
}


// checks if the directory is on a Linux partition
bool MainWindow::isOnSupportedPart(QDir dir)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QStringList supported_partitions = (QStringList() << "ext2/ext3" << "btrfs" << "jfs" << "reiserfs" << "xfs" << "fuseblk"); // supported partition types (NTFS returns fuseblk)
    QString part_type = shell->getCmdOut("stat --file-system --format=%T \"" + dir.absolutePath() + "\"").trimmed();
    qDebug() << "detected partition" << part_type << "supported part:" << supported_partitions.contains(part_type);
    return supported_partitions.contains(part_type);
}

// Check if running from a 32bit environment
bool MainWindow::isi686()
{
    return (shell->getCmdOut("uname -m") == "i686");
}

// return number of snapshots in snapshot_dir
int MainWindow::getSnapshotCount()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (snapshot_dir.exists()) {
        QFileInfoList list = snapshot_dir.entryInfoList(QStringList("*.iso"), QDir::Files);
        return list.size();
    }
    return 0;
}

// return the size of the work folder
QString MainWindow::getSnapshotSize()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QString size;
    if (snapshot_dir.exists()) {
        QString cmd = QString("/usr/bin/find \"%1\" -maxdepth 1 -type f -name '*.iso' -exec du -shc {} + | tail -1 | awk '{print $1}'").arg(snapshot_dir.absolutePath());
        size = shell->getCmdOut(cmd);
        if (!size.isEmpty()) {
            return size;
        }
    }
    return "0";
}

// return the XDG User Directory for each user with different localizations than English
QString MainWindow::getXdgUserDirs(const QString& folder)
{
    QString result = "";

    foreach (const QString &user, users) {
        QByteArray out;
        bool success = shell->run("runuser -l " + user + " -c \"/usr/bin/xdg-user-dir " + folder + "\"", out);
        QString dir = QString(out);
        if (success) {
            if (englishDirs.value(folder) == dir.section("/", -1) || dir.trimmed() == "/home/" + user || dir.trimmed() == "/home/" + user + "/" || dir.isEmpty()) { // skip if English name or of return folder is the home folder (if XDG-USER-DIR not defined)
                continue;
            }
            if (dir.startsWith("/")) {
                dir.remove(0, 1); // remove training slash
            }
            (folder == "DESKTOP") ? dir.append("/!(minstall.desktop)") : dir.append("/*\" \"" + dir + "/.*");
            (result.isEmpty()) ? result.append("\" \"" + dir) : result.append(" \"" + dir);
            result.append("\""); // close the quote for each user, will strip the last one before returning;
        }
    }
    result.chop(1); // chop the last quote, will be added later on in addRemoveExclusion
    return result;
}

// return a list of users that have folders in /home
QStringList MainWindow::listUsers()
{
    return shell->getCmdOut("/usr/bin/lslogins --noheadings -u -o user | grep -vw root").split("\n");
}

// List used space
void MainWindow::listUsedSpace()
{
    this->show();
    ui->buttonNext->setDisabled(true);
    ui->buttonCancel->setDisabled(true);
    ui->buttonSelectSnapshot->setDisabled(true);
    QString cmd;
    QString out = "\n- " + tr("Used space on / (root): ");
    if (live) {
        out += getLiveRootSpace() + "GB" + " -- " + tr("estimated");
    } else {
        cmd = QString("df -h / | /usr/bin/awk 'NR==2 {print $3}'");
        out += shell->getCmdOut(cmd);
    }
    if (shell->run("mountpoint -q /home")) {
        cmd = QString("df -h /home | /usr/bin/awk 'NR==2 {print $3}'");
        out.append("\n- " + tr("Used space on /home: ") + shell->getCmdOut(cmd));
    }
    ui->buttonNext->setEnabled(true);
    ui->buttonCancel->setEnabled(true);
    ui->buttonSelectSnapshot->setEnabled(true);
    ui->labelUsedSpace->setText(out);
}


// List free space on drives
void MainWindow::listFreeSpace()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QString cmd;
    QString out;
    QString path = snapshot_dir.absolutePath().remove("/snapshot");
    cmd = QString("df -h \"%1\" | /usr/bin/awk 'NR==2 {print $4}'").arg(path);
    ui->labelFreeSpace->clear();
    out.append("- " + tr("Free space on %1, where snapshot folder is placed: ").arg(path) + shell->getCmdOut(cmd) + "\n");
    ui->labelFreeSpace->setText(out);
    ui->labelDiskSpaceHelp->setText(tr("The free space should be sufficient to hold the compressed data from / and /home\n\n"
                                       "      If necessary, you can create more available space\n"
                                       "      by removing previous snapshots and saved copies:\n"
                                       "      %1 snapshots are taking up %2 of disk space.\n").arg(QString::number(getSnapshotCount())).arg(getSnapshotSize()));
}

// Checks if package is installed
bool MainWindow::checkInstalled(QString package)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QString cmd = QString("/usr/bin/dpkg -s %1 | grep Status").arg(package);
    if (shell->getCmdOut(cmd) == "Status: install ok installed") {
        return true;
    }
    return false;
}

// Installs package
bool MainWindow::installPackage(QString package)
{
    this->setWindowTitle(tr("Installing ") + package);
    ui->outputLabel->setText(tr("Installing ") + package);
    ui->outputBox->clear();
    ui->buttonNext->setDisabled(true);
    ui->buttonBack->setDisabled(true);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    displayOutput();
    shell->run("/usr/bin/apt-get update");
    if (!shell->run("/usr/bin/apt-get install -y " + package)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not install ") + package);
        disableOutput();
        return false;
    }
    disableOutput();
    return true;
}

void MainWindow::checkDirectories()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    //  Create snapshot dir if it doesn't exist
    if (!snapshot_dir.exists()) {
        snapshot_dir.mkpath(snapshot_dir.absolutePath());
    }
    // Create a work_dir
    QString parent_dir = snapshot_dir.absolutePath();
    if (!isOnSupportedPart(snapshot_dir)) { // if not saving snapshot on a Linux partition put working dir in /home
        parent_dir = largerFreeSpace("/tmp", "/home");
    } else {
        parent_dir = largerFreeSpace("/tmp", "/home", snapshot_dir.absolutePath());
    }
    work_dir = shell->getCmdOut("mktemp -d \"" + parent_dir + "/mx-snapshot-XXXXXXXX\"");
    system("mkdir -p " + work_dir.toUtf8() + "/iso-template/antiX");
    system("cd ..; cd -");
}

void MainWindow::openInitrd(QString file, QString initrd_dir)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    ui->outputLabel->setText(tr("Building new initrd..."));
    QString cmd = "chmod a+rx \"" + initrd_dir + "\"";
    shell->run(cmd);
    QDir::setCurrent(initrd_dir);
    cmd = QString("gunzip -c \"%1\" | cpio -idum").arg(file);
    shell->run(cmd);
}

void MainWindow::closeInitrd(QString initrd_dir, QString file)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QDir::setCurrent(initrd_dir);
    QString cmd = "(/usr/bin/find . | cpio -o -H newc --owner root:root | gzip -9) >\"" + file + "\"";
    shell->run(cmd);
    if (initrd_dir.startsWith("/tmp/tmp.")) {
        shell->run("rm -r " + initrd_dir);
    }
    makeMd5sum(work_dir + "/iso-template/antiX", "initrd.gz");
}

// Copying the iso-template filesystem
void MainWindow::copyNewIso()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    ui->outputBox->clear();

    ui->outputLabel->setText(tr("Copying the new-iso filesystem..."));
    QDir::setCurrent(work_dir);

    QString cmd = "tar xf /usr/lib/iso-template/iso-template.tar.gz";
    shell->run(cmd);

    cmd = "cp /usr/lib/iso-template/template-initrd.gz iso-template/antiX/initrd.gz";
    shell->run(cmd);

    cmd = "cp /boot/vmlinuz-" + kernel_used + " iso-template/antiX/vmlinuz";
    shell->run(cmd);

    if (debian_version < 9) { // Only for versions older than Stretch
        if (i686) {
            shell->run("cp /boot/vmlinuz-3.16.0-4-586 iso-template/antiX/vmlinuz1");
        } else {
            // mv x64 template files over
            shell->run("mv iso-template/boot/grub/grub.cfg_x64 iso-template/boot/grub/grub.cfg");
            shell->run("mv iso-template/boot/syslinux/syslinux.cfg_x64 iso-template/boot/syslinux/syslinux.cfg");
            shell->run("mv iso-template/boot/isolinux/isolinux.cfg_x64 iso-template/boot/isolinux/isolinux.cfg");
        }
    }

    replaceMenuStrings();
    makeMd5sum(work_dir + "/iso-template/antiX", "vmlinuz");

    QString initrd_dir = shell->getCmdOut("mktemp -d");
    openInitrd(work_dir + "/iso-template/antiX/initrd.gz", initrd_dir);
    if (initrd_dir.startsWith("/tmp/tmp.")) {  //just make sure initrd_dir is correct to avoid disaster
        // strip modules
        shell->run("/usr/bin/test -d \"" + initrd_dir + "/lib/modules\" && rm -r \"" + initrd_dir  + "/lib/modules\"");
    }
    shell->run("/usr/bin/test -r /usr/local/share/live-files/files/etc/initrd-release && cp /usr/local/share/live-files/files/etc/initrd-release \"" + initrd_dir + "/etc\""); // We cannot count on this file in the future versions
    shell->run("/usr/bin/test -r /etc/initrd-release && cp /etc/initrd-release \"" + initrd_dir + "/etc\""); // overwrite with this file, probably a better location _if_ the file exists
    if (!initrd_dir.isEmpty()) {
        copyModules(initrd_dir, kernel_used);
        closeInitrd(initrd_dir, work_dir + "/iso-template/antiX/initrd.gz");
    }
}

// replace text in menu items in grub.cfg, syslinux.cfg, isolinux.cfg
void MainWindow::replaceMenuStrings() {
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QString date = QDate::currentDate().toString("dd MMMM yyyy");
    QString distro = shell->getCmdOut("cat /etc/antix-version | /usr/bin/cut -f1 -d'_'");
    QString distro_name = shell->getCmdOut("grep -oP '(?<=DISTRIB_ID=).*' /etc/lsb-release");
    QString full_distro_name = shell->getCmdOut("cat /etc/antix-version | /usr/bin/cut -f-2 -d' '");
    QString code_name = shell->getCmdOut("grep -oP '(?<=DISTRIB_CODENAME=).*' /etc/lsb-release");
    QString options = "quiet";

    if (debian_version < 9) { // Only for versions older than Stretch which uses old mx-iso-template
        if (i686) {
            QString new_string = "MX Linux 386 (" + date + ")";
            replaceStringInFile("custom-name", new_string, work_dir + "/iso-template/boot/grub/grub.cfg");
            replaceStringInFile("custom-name", new_string, work_dir + "/iso-template/boot/syslinux/syslinux.cfg");
            replaceStringInFile("custom-name", new_string, work_dir + "/iso-template/boot/isolinux/isolinux.cfg");
        } else {
            QString new_string = "MX Linux x64 (" + date + ")";
            replaceStringInFile("custom-name", new_string, work_dir + "/iso-template/boot/grub/grub.cfg");
            replaceStringInFile("custom-name", new_string, work_dir + "/iso-template/boot/syslinux/syslinux.cfg");
            replaceStringInFile("custom-name", new_string, work_dir + "/iso-template/boot/isolinux/isolinux.cfg");
        }

    } else { // with new mx-iso-template for MX-17 and greater
        replaceStringInFile("%DISTRO_NAME%", distro_name, work_dir + "/iso-template/boot/grub/grub.cfg");

        replaceStringInFile("%OPTIONS%", options, work_dir + "/iso-template/boot/syslinux/syslinux.cfg");
        replaceStringInFile("%OPTIONS%", options, work_dir + "/iso-template/boot/isolinux/isolinux.cfg");

        replaceStringInFile("%FULL_DISTRO_NAME%", full_distro_name, work_dir + "/iso-template/boot/grub/grub.cfg");
        replaceStringInFile("%FULL_DISTRO_NAME%", full_distro_name, work_dir + "/iso-template/boot/syslinux/syslinux.cfg");
        replaceStringInFile("%FULL_DISTRO_NAME%", full_distro_name, work_dir + "/iso-template/boot/syslinux/readme.msg");
        replaceStringInFile("%FULL_DISTRO_NAME%", full_distro_name, work_dir + "/iso-template/boot/isolinux/isolinux.cfg");
        replaceStringInFile("%FULL_DISTRO_NAME%", full_distro_name, work_dir + "/iso-template/boot/isolinux/readme.msg");

        replaceStringInFile("%DISTRO%", distro, work_dir + "/iso-template/boot/grub/theme/theme.txt");
        replaceStringInFile("%DISTRO%", distro, work_dir + "/iso-template/boot/grub/grub.cfg");

        replaceStringInFile("%RELEASE_DATE%", date, work_dir + "/iso-template/boot/grub/grub.cfg");
        replaceStringInFile("%RELEASE_DATE%", date, work_dir + "/iso-template/boot/syslinux/syslinux.cfg");
        replaceStringInFile("%RELEASE_DATE%", date, work_dir + "/iso-template/boot/syslinux/readme.msg");
        replaceStringInFile("%RELEASE_DATE%", date, work_dir + "/iso-template/boot/isolinux/isolinux.cfg");
        replaceStringInFile("%RELEASE_DATE%", date, work_dir + "/iso-template/boot/isolinux/readme.msg");

        replaceStringInFile("%CODE_NAME%", code_name, work_dir + "/iso-template/boot/syslinux/syslinux.cfg");
        replaceStringInFile("%CODE_NAME%", code_name, work_dir + "/iso-template/boot/isolinux/isolinux.cfg");
        replaceStringInFile("%ASCII_CODE_NAME%", code_name, work_dir + "/iso-template/boot/grub/theme/theme.txt");
    }
}

// copyModules(mod_dir/kernel_used kernel_used)
void MainWindow::copyModules(QString to, QString kernel)
{
    QString kernel586 = "3.16.0-4-586";
    QString cmd = QString("/usr/share/mx-packageinstaller/scripts/copy-initrd-modules -t=\"%1\" -k=\"%2\"").arg(to).arg(kernel);
    shell->run(cmd);
    // copy 586 modules for the non-PAE kernel
    if (i686 && debian_version < 9) {  // Not applicable for Stretch (MX17) or more
        QString cmd = QString("/usr/share/mx-packageinstaller/scripts/copy-initrd-modules -t=\"%1\" -k=\"%2\"").arg(to).arg(kernel586);
        shell->run(cmd);
    }
    cmd = QString("/usr/share/mx-packageinstaller/scripts/copy-initrd-programs --to=\"%1\"").arg(to);
    shell->run(cmd);
}

// Create the output filename
QString MainWindow::getFilename()
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
            dir.setPath("\"" + snapshot_dir.absolutePath() + "/" + name + "\"");
            n++;
        } while (dir.exists(dir.absolutePath()));
        return name;
    }
}

// return the directory that has more free space available
QString MainWindow::largerFreeSpace(QString dir1, QString dir2)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    int dir1_free = shell->getCmdOut("df -k --output=avail \"" + dir1 + "\" 2>/dev/null | tail -n1").toInt();
    int dir2_free = shell->getCmdOut("df -k --output=avail \"" + dir2 + "\" 2>/dev/null | tail -n1").toInt();

    if (dir1_free >= dir2_free) {
        return dir1;
    } else {
        return dir2;
    }
}

// return the directory that has more free space available
QString MainWindow::largerFreeSpace(QString dir1, QString dir2, QString dir3)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    return largerFreeSpace(largerFreeSpace(dir1, dir2), dir3);
}

QString MainWindow::getEditor()
{
    QString editor;
    if (system("command -v " + gui_editor.fileName().toUtf8()) != 0) {  // if specified editor doesn't exist get the default one
        editor = shell->getCmdOut("grep Exec $(locate $(xdg-mime query default text/plain))|cut -d= -f2|cut -d' ' -f1|head -1");
        if (editor == "kate" || editor == "kwrite") { // need to run these as normal user
            editor = "runuser -u $(logname) " + editor;
        }
        if (editor.isEmpty() || system("command -v " + editor.toUtf8()) != 0) { // if default one doesn't exit use nano as backup editor
            editor = "x-terminal-emulator -e nano";
        }
    } else {
        editor = gui_editor.fileName();
    }
    return editor;
}

// make working directory using the base filename
void MainWindow::mkDir(QString file_name)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QDir dir;
    QFileInfo fi(file_name);
    QString base_name = fi.completeBaseName(); // remove extension
    dir.setPath(work_dir + "/iso-template/" + base_name);
    dir.mkpath(dir.absolutePath());
}

// save package list in working directory
void MainWindow::savePackageList(QString file_name)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QFileInfo fi(file_name);
    QString base_name = fi.completeBaseName(); // remove extension
    QString full_name = work_dir + "/iso-template/" + base_name + "/package_list";
    QString cmd = "/usr/bin/dpkg -l | grep ^ii\\ \\ | /usr/bin/awk '{print $2,$3}' | sed 's/:'$(dpkg --print-architecture)'//' | /usr/bin/column -t >\"" + full_name + "\"";
    shell->run(cmd);
}

// setup the environment before taking the snapshot
void MainWindow::setupEnv()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    // checks if work_dir looks OK
    if (!work_dir.contains("/mx-snapshot")) {
        return;
    }

    QString bind_boot = "";
    QString bind_boot_too = "";
    if (shell->run("mountpoint /boot")) {
        bind_boot = "bind=/boot ";
        bind_boot_too = ",/boot";
    }

    // install mx-installer if absent
    if (force_installer && !checkInstalled("mx-installer")) {
        installPackage("mx-installer");
    }

    writeSnapshotInfo();

    // setup environment if creating a respin (reset root/demo, remove personal accounts)
    if (reset_accounts) {
        shell->run("installed-to-live -b /.bind-root start " + bind_boot + "empty=/home general version-file read-only");
    } else {
        if (force_installer == true) {  // copy minstall.desktop to Desktop on all accounts
            shell->run("echo /home/*/Desktop | /usr/bin/xargs -n1 cp /usr/share/applications/minstall.desktop 2>/dev/null");
            shell->run("chmod a+rwx /home/*/Desktop/minstall.desktop"); //removes lock symbol on installer on desktop
        }
        shell->run("installed-to-live -b /.bind-root start bind=/home" + bind_boot_too + " live-files version-file adjtime read-only");
    }
}

// write date of the snapshot in a "snapshot_created" file
void MainWindow::writeSnapshotInfo()
{
    QFile file("/usr/local/share/live-files/files/etc/snapshot_created");
    if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
        return;
    }
    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString("yyyyMMdd_HHmm");
    file.close();
}

QString MainWindow::getLiveRootSpace()
{
    //rootspaceneeded is the size of the linuxfs file * a compression factor + contents of the rootfs.  conservative but fast
    //factors are same as used in live-remaster

    //load some live variables
    QSettings livesettings("/live/config/initrd.out", QSettings::NativeFormat);
    QString sqfile_full = livesettings.value("SQFILE_FULL", "/live/boot-dev/antiX/linuxfs").toString();

    //get compression factor by reading the linuxfs squasfs file, if available
    QString linuxfs_compression_type = shell->getCmdOut("dd if=" + sqfile_full + " bs=1 skip=20 count=2 status=none 2>/dev/null| /usr/bin/od -An -tdI");
    ushort compression_factor;
    //gzip, xz, or lz4
    if ( linuxfs_compression_type == "1") {
        compression_factor = 37; // gzip
    } else if (linuxfs_compression_type == "2") {
        compression_factor = 52; //lzo, not used by antiX
    } else if (linuxfs_compression_type == "3") {
        compression_factor = 52;  //lzma, not used by antiX
    } else if (linuxfs_compression_type == "4") {
        compression_factor = 31; //xz
    } else if (linuxfs_compression_type == "5") {
        compression_factor = 52; // lz4
    } else {
        compression_factor = 30; //anything else or linuxfs not reachable (toram), should be pretty conservative
    }

    qlonglong rootfs_file_size = 0;
    qlonglong linuxfs_file_size = shell->getCmdOut("df /live/linux --output=used --total | /usr/bin/tail -n1").toLongLong() * 1024 * 100 / compression_factor;
    if (QFileInfo::exists("/live/perist-root")) {
        rootfs_file_size = shell->getCmdOut("df /live/persist-root --output=used --total | /usr/bin/tail -n1").toLongLong() * 1024;
    }

    //add rootfs file size to the calculated linuxfs file size.  probaby conservative, as rootfs will likely have some overlap with linuxfs
    qlonglong rootSpaceNeeded = linuxfs_file_size + rootfs_file_size;
    return QString::number(rootSpaceNeeded / 1073741824.0, 'f', 1); // convert to GB
}

int MainWindow::getDebianVersion()
{
    return shell->getCmdOut("cat /etc/debian_version | /usr/bin/cut -f1 -d'.'").toInt();
}


// create squashfs and then the iso
bool MainWindow::createIso(QString filename)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    // add exclusions snapshot and work dirs
    addRemoveExclusion(true, snapshot_dir.absolutePath());
    addRemoveExclusion(true, work_dir);

    if (reset_accounts) {
        addRemoveExclusion(true, "/etc/minstall.conf");
        // exclude /etc/localtime if link and timezone not America/New_York
        if (shell->run("/usr/bin/test -L /etc/localtime") && shell->getCmdOut("cat /etc/timezone") != "America/New_York" ) {
            addRemoveExclusion(true, "/etc/localtime");
        }
    }

    // squash the filesystem copy
    QDir::setCurrent(work_dir);
    QString cmd;
    cmd = "/usr/bin/mksquashfs /.bind-root iso-template/antiX/linuxfs -comp " + compression + ((mksq_opt.isEmpty()) ? "" : " " + mksq_opt) + " -wildcards -ef " + snapshot_excludes.fileName() + " " + session_excludes;

    ui->outputLabel->setText(tr("Squashing filesystem..."));
    displayOutput();
    if (!shell->run(cmd)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not create linuxfs file, please check whether you have enough space on the destination partition."));
        return false;
    }
    makeMd5sum(work_dir + "/iso-template/antiX", "linuxfs");

    // mv linuxfs to another folder
    system("mkdir -p iso-2/antiX");
    shell->run("mv iso-template/antiX/linuxfs* iso-2/antiX");

    shell->run("installed-to-live cleanup");

    // create the iso file
    QDir::setCurrent(work_dir + "/iso-template");
    cmd = "/usr/bin/xorriso -as mkisofs -l -V MXLIVE -R -J -pad -iso-level 3 -no-emul-boot -boot-load-size 4 -boot-info-table -b boot/isolinux/isolinux.bin  -eltorito-alt-boot -e boot/grub/efi.img -no-emul-boot -c boot/isolinux/isolinux.cat -o \"" + snapshot_dir.absolutePath() + "/" + filename + "\" . \""  + work_dir + "/iso-2\"";
    ui->outputLabel->setText(tr("Creating CD/DVD image file..."));
    if (!shell->run(cmd)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not create ISO file, please check whether you have enough space on the destination partition."));
        disableOutput();
        return false;
    }

    // make it isohybrid
    if (make_isohybrid == "yes") {
        ui->outputLabel->setText(tr("Making hybrid iso"));
        cmd = "/usr/bin/isohybrid --uefi \"" + snapshot_dir.absolutePath() + "/" + filename + "\"";
        shell->run(cmd);
    }

    // make md5sum
    if (make_chksum == "yes") {
        makeMd5sum(snapshot_dir.absolutePath(), filename);
        makeSha512sum(snapshot_dir.absolutePath(), filename);
    }

    QTime time(0, 0);
    time = time.addMSecs(e_timer.elapsed());
    outputAvailable("\n" + tr("MX Snapshot completed sucessfully!") + "\n");
    outputAvailable(tr("Snapshot took %1 to finish.").arg(time.toString("hh:mm:ss")) + "\n");
    qDebug() << tr("Snapshot took %1 to finish.").arg(time.toString("hh:mm:ss"));
    outputAvailable(tr("Thanks for using MX Snapshot, run MX Live USB Maker next!"));
    disableOutput();
    return true;
}

// create md5sum for different files
void MainWindow::makeMd5sum(QString folder, QString file_name)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    shell->run("sync");
    QDir dir;
    QString current = dir.currentPath();
    dir.setCurrent(folder);
    ui->outputLabel->setText(tr("Making md5sum"));
    QString cmd = "/usr/bin/md5sum \"" + file_name + "\">\"" + folder + "/" + file_name + ".md5\"";
    shell->run(cmd);
    dir.setCurrent(current);
}

// create sha512sum
void MainWindow::makeSha512sum(QString folder, QString file_name)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    shell->run("sync");
    QDir dir;
    QString current = dir.currentPath();
    dir.setCurrent(folder);
    QString cmd = "/usr/bin/sha512sum \"" + file_name + "\">\"" + folder + "/" + file_name + ".sha512\"";
    shell->run(cmd);
    dir.setCurrent(current);
}

// clean up changes before exit
void MainWindow::cleanUp()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    ui->outputLabel->setText(tr("Cleaning..."));
    shell->run("sync");
    system("/usr/bin/pkill mksquashfs; /usr/bin/pkill md5sum");
    QDir::setCurrent("/");
    system("/usr/bin/[ -f /tmp/installed-to-live/cleanup.conf ] && installed-to-live cleanup");

    // checks if work_dir looks OK
    if (work_dir.contains("/mx-snapshot")) {
        system("rm -r \"" + work_dir.toUtf8() + "\"");
    }
    if (!live && !reset_accounts) {
        // remove installer icon
        system("rm /home/*/Desktop/minstall.desktop");
    }
    ui->outputLabel->setText(tr("Done"));
}

// adds or removes exclusion to the exclusion string
void MainWindow::addRemoveExclusion(bool add, QString exclusion)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    if (exclusion.startsWith("/")) {
        exclusion.remove(0, 1); // remove preceding slash
    }
    if (add) {
        if (session_excludes.isEmpty()) {
            session_excludes.append("-e \"" + exclusion + "\"");
        } else {
            session_excludes.append(" \"" + exclusion + "\"");
        }
    } else {
        session_excludes.remove(" \"" + exclusion + "\"");
        if (session_excludes == "-e") {
            session_excludes = "";
        }
    }
}

// check if compression is available in the kernel (lz4, lzo, xz)
bool MainWindow::checkCompression()
{
    if (!shell->run("/usr/bin/[ -f /boot/config-$(uname -r) ]")) { // return true if cannot check config file
        return true;
    }
    if (compression == "lz4") {
        return (shell->run("grep ^CONFIG_SQUASHFS_LZ4=y /boot/config-$(uname -r)"));
    } else if (compression == "xz") {
        return (shell->run("grep ^CONFIG_SQUASHFS_XZ=y /boot/config-$(uname -r)"));
    } else if (compression == "lzo") {
        return (shell->run("grep ^CONFIG_SQUASHFS_LZO=y /boot/config-$(uname -r)"));
    } else {
        return true;
    }
}

//// sync process events ////
void MainWindow::procStart()
{
    timer.start(500);
    setCursor(QCursor(Qt::BusyCursor));
}

void MainWindow::procDone()
{
    timer.stop();
    ui->progressBar->setValue(ui->progressBar->maximum());
    setCursor(QCursor(Qt::ArrowCursor));
}

void MainWindow::displayOutput()
{
    connect(shell, &Cmd::outputAvailable, this, &MainWindow::outputAvailable);
    connect(shell, &Cmd::errorAvailable, this, &MainWindow::outputAvailable);
}

void MainWindow::disableOutput()
{
    disconnect(shell, &Cmd::outputAvailable, this, &MainWindow::outputAvailable);
    disconnect(shell, &Cmd::errorAvailable, this, &MainWindow::outputAvailable);
}

// update output box
void MainWindow::outputAvailable(const QString &output)
{
    if (output.contains("\r")) {
        ui->outputBox->moveCursor(QTextCursor::Up, QTextCursor::KeepAnchor);
        ui->outputBox->moveCursor(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    }
    ui->outputBox->insertPlainText(output);
    ui->outputBox->verticalScrollBar()->setValue(ui->outputBox->verticalScrollBar()->maximum());
}

void MainWindow::progress()
{
    if (ui->progressBar->value() == 100) {
        ui->progressBar->reset();
    }
    ui->progressBar->setValue(ui->progressBar->value() + 1);

    // in live environment and first page, blink text while calculating used disk space
    if (live && (ui->stackedWidget->currentIndex() == 0)) {
        if (ui->progressBar->value()%4 == 0 ) {
            ui->labelUsedSpace->setText("\n " + tr("Please wait."));
        } else {
            ui->labelUsedSpace->setText("\n " + tr("Please wait. Calculating used disk space..."));
        }
    }
}


// Next button clicked
void MainWindow::on_buttonNext_clicked()
{
    QString file_name = ui->lineEditName->text();
    if (!file_name.endsWith(".iso")) {
        file_name += ".iso";
    }
    // on first page
    if (ui->stackedWidget->currentIndex() == 0) {
        this->setWindowTitle(tr("Settings"));
        ui->stackedWidget->setCurrentWidget(ui->settingsPage);
        ui->buttonBack->setHidden(false);
        ui->buttonBack->setEnabled(true);
        kernel_used = shell->getCmdOut("uname -r");
        ui->stackedWidget->setCurrentWidget(ui->settingsPage);
        ui->label_1->setText(tr("Snapshot will use the following settings:*"));

        ui->label_2->setText("\n" + tr("- Snapshot directory:") + " " + snapshot_dir.absolutePath() + "\n" +
                       "- " + tr("Snapshot name:") + " " + file_name + "\n" +
                       tr("- Kernel to be used:") + " " + kernel_used + "\n");

    // on settings page
    } else if (ui->stackedWidget->currentWidget() == ui->settingsPage) {
        if (!checkCompression()) {
            QMessageBox::critical(this, tr("Error"),
                    tr("Current kernel doesn't support selected compression algorithm, please edit the configuration file and select a different algorithm."));
            return;
        }

        int ans = QMessageBox::question(this, tr("Final chance"),
                              tr("Snapshot now has all the information it needs to create an ISO from your running system.") + "\n\n" +
                              tr("It will take some time to finish, depending on the size of the installed system and the capacity of your computer.") + "\n\n" +
                              tr("OK to start?"), QMessageBox::Ok | QMessageBox::Cancel);
        if (ans == QMessageBox::Cancel) {
            return;
        }
        e_timer.start();
        checkDirectories();
        ui->buttonNext->setEnabled(false);
        ui->buttonBack->setEnabled(false);
        ui->stackedWidget->setCurrentWidget(ui->outputPage);
        this->setWindowTitle(tr("Output"));
        copyNewIso();
        ui->outputLabel->setText("");
        mkDir(file_name);
        savePackageList(file_name);

        if (edit_boot_menu == "yes") {
            ans = QMessageBox::question(this, tr("Edit Boot Menu"),
                                  tr("The program will now pause to allow you to edit any files in the work directory. Select Yes to edit the boot menu or select No to bypass this step and continue creating the snapshot."),
                                     QMessageBox::Yes | QMessageBox::No);
            if (ans == QMessageBox::Yes) {
                this->hide();
                QString cmd = getEditor() + " \"" + work_dir + "/iso-template/boot/isolinux/isolinux.cfg\"";
                shell->run(cmd);
                this->show();
            }
        }
        setupEnv();
        bool success = createIso(file_name);
        cleanUp();
        if (success) {
            QMessageBox::information(this, tr("Success"),tr("All finished!"), QMessageBox::Ok);
            ui->buttonCancel->setText(tr("Close"));
        }
    } else {
        return qApp->quit();
    }
}

void MainWindow::on_buttonBack_clicked()
{
    this->setWindowTitle(tr("MX Snapshot"));
    ui->stackedWidget->setCurrentIndex(0);
    ui->buttonNext->setEnabled(true);
    ui->buttonBack->setHidden(true);
    ui->outputBox->clear();
}

void MainWindow::on_buttonEditExclude_clicked()
{
    this->hide();
    shell->run(getEditor() + " " + snapshot_excludes.fileName());
    this->show();
}

void MainWindow::on_excludeDocuments_toggled(bool checked)
{
    QString folder = "home/*/Documents/";
    QString xdg_name = "DOCUMENTS";
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(checked, exclusion);
    if (!checked) {
        ui->excludeAll->setChecked(false);
    }
}

void MainWindow::on_excludeDownloads_toggled(bool checked)
{
    QString folder = "home/*/Downloads/";
    QString xdg_name = "DOWNLOAD";
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(checked, exclusion);
    if (!checked) {
        ui->excludeAll->setChecked(false);
    }
}

void MainWindow::on_excludePictures_toggled(bool checked)
{
    QString folder = "home/*/Pictures/";
    QString xdg_name = "PICTURES";
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(checked, exclusion);
    if (!checked) {
        ui->excludeAll->setChecked(false);
    }
}

void MainWindow::on_excludeMusic_toggled(bool checked)
{
    QString folder = "home/*/Music/";
    QString xdg_name = "MUSIC";
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(checked, exclusion);
    if (!checked) {
        ui->excludeAll->setChecked(false);
    }
}

void MainWindow::on_excludeVideos_toggled(bool checked)
{
    QString folder = "home/*/Videos/";
    QString xdg_name = "VIDEOS";
    QString exclusion = folder + "*\" \"" + folder + ".*" + getXdgUserDirs(xdg_name);
    addRemoveExclusion(checked, exclusion);
    if (!checked) {
        ui->excludeAll->setChecked(false);
    }
}

void MainWindow::on_excludeDesktop_toggled(bool checked)
{
    QString exclusion = "/home/*/Desktop/!(minstall.desktop)" + getXdgUserDirs("DESKTOP");
    addRemoveExclusion(checked, exclusion);
    if (!checked) {
        ui->excludeAll->setChecked(false);
    }
}

void MainWindow::on_radioRespin_clicked(bool checked)
{
    if (checked) {
        reset_accounts = true;
        if (!ui->excludeAll->isChecked()) {
            ui->excludeAll->click();
        }
    }
}

void MainWindow::on_radioPersonal_clicked(bool checked)
{
    if (checked) {
        if (ui->excludeAll->isChecked()) {
            ui->excludeAll->click();
        }
    }
}


// About button clicked
void MainWindow::on_buttonAbout_clicked()
{
    this->hide();
    displayAboutMsgBox(tr("About %1").arg(this->windowTitle()), "<p align=\"center\"><b><h2>" + this->windowTitle() +"</h2></b></p><p align=\"center\">" +
                       tr("Version: ") + VERSION + "</p><p align=\"center\"><h3>" +
                       tr("Program for creating a live-CD from the running system for MX Linux") +
                       "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p><p align=\"center\">" +
                       tr("Copyright (c) MX Linux") + "<br /><br /></p>",
                       "/usr/share/doc/mx-snapshot/license.html", tr("%1 License").arg(this->windowTitle()), true);
    this->show();
}

// Help button clicked
void MainWindow::on_buttonHelp_clicked()
{
    QLocale locale;
    QString lang = locale.bcp47Name();

    QString url = "/usr/share/doc/mx-snapshot/help/mx-snapshot.html";

    if (lang.startsWith("fr")) {
        url = "https://mxlinux.org/french-wiki/help-files-fr/help-mx-instantane";
    }
    displayDoc(url, tr("%1 Help").arg(this->windowTitle()), true);
}

// Select snapshot directory
void MainWindow::on_buttonSelectSnapshot_clicked()
{
    QFileDialog dialog;

    QString selected = dialog.getExistingDirectory(this, tr("Select Snapshot Directory"), QString(), QFileDialog::ShowDirsOnly);
    if (!selected.isEmpty()) {
        snapshot_dir.setPath(selected + "/snapshot");
        ui->labelSnapshotDir->setText(snapshot_dir.absolutePath());
        listFreeSpace();
    }
}

// process keystrokes
void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        closeApp();
    }
}

// close application
void MainWindow::closeApp() {
    // ask for confirmation when on outputPage and not done
    if (ui->stackedWidget->currentWidget() == ui->outputPage && ui->outputLabel->text() != tr("Done")) {
        int ans = QMessageBox::question(this, tr("Confirmation"), tr("Are you sure you want to quit the application?"),
                                        QMessageBox::Yes | QMessageBox::No);
        if (ans == QMessageBox::Yes) {
            return qApp->quit();
        }
    } else {
        return qApp->quit();
    }
}

void MainWindow::on_buttonCancel_clicked()
{
    closeApp();
}

void MainWindow::on_cbCompression_currentIndexChanged(const QString &arg1)
{
    QSettings settings(config_file.fileName(), QSettings::IniFormat);
    settings.setValue("compression", arg1);
    compression = arg1;
}

void MainWindow::on_excludeNetworks_toggled(bool checked)
{
    QString exclusion = "/etc/NetworkManager/system-connections/*";
    addRemoveExclusion(checked, exclusion);
    if (!checked) {
        ui->excludeAll->setChecked(false);
    }
}
