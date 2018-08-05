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


#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QScrollBar>
#include <QTextStream>
#include <QKeyEvent>

#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    shell = new Cmd(this);
    connect(shell, &Cmd::started, this, &MainWindow::procStart);
    connect(shell, &Cmd::finished, this, &MainWindow::procDone);
    connect(shell, &Cmd::runTime, this, &MainWindow::progress);
    connect(shell, &Cmd::outputAvailable, [](QString out) {qDebug() << out;});
    connect(shell, &Cmd::errorAvailable, [](QString out) {qWarning() << out;});

    this->setWindowTitle(tr("MX Snapshot"));
    ui->buttonBack->setHidden(true);
    ui->buttonSelectSnapshot->setHidden(true);
    ui->stackedWidget->setCurrentIndex(0);

    version = getVersion("mx-snapshot");
    live = isLive();

    i686 = isi686();
    debian_version = getDebianVersion();
    setup();
    reset_accounts = false;
    listUsedSpace();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// load settings or use the default value
void MainWindow::loadSettings()
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    config_file.setFileName("/etc/mx-snapshot.conf");
    QSettings settings(config_file.fileName(), QSettings::IniFormat);

    session_excludes = "";
    snapshot_dir = settings.value("snapshot_dir", "/home/snapshot").toString();
    ui->labelSnapshotDir->setText(snapshot_dir.absolutePath());
    snapshot_excludes.setFileName(settings.value("snapshot_excludes", "/usr/local/share/excludes/mx-snapshot-exclude.list").toString());
    snapshot_basename = settings.value("snapshot_basename", "snapshot").toString();
    make_md5sum = settings.value("make_md5sum", "no").toString();
    make_isohybrid = settings.value("make_isohybrid", "yes").toString();
    mksq_opt = settings.value("mksq_opt", "-comp xz").toString();
    edit_boot_menu = settings.value("edit_boot_menu", "no").toString();
    lib_mod_dir = settings.value("lib_mod_dir", "/lib/modules/").toString();
    gui_editor.setFileName(settings.value("gui_editor", "/usr/bin/gnome-text-editor").toString());
    stamp = settings.value("stamp").toString();
    force_installer = settings.value("force_installer", "true").toBool();
    ui->lineEditName->setText(getFilename());
}

// setup/refresh versious items first time program runs
void MainWindow::setup()
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
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
}


// Util function for replacing strings in files
bool MainWindow::replaceStringInFile(QString old_text, QString new_text, QString file_path)
{
    QString cmd = QString("sed -i 's/%1/%2/g' \"%3\"").arg(old_text).arg(new_text).arg(file_path);
    if (shell->run(cmd) != 0) {
        return false;
    }
    return true;
}

// Check if running from a live envoronment
bool MainWindow::isLive()
{
    return (shell->run("mountpoint -q /live/aufs") == 0 );
}

// Check if running from a 32bit environment
bool MainWindow::isi686()
{
    return (shell->getOutput("uname -m") == "i686");
}

// Get version of the program
QString MainWindow::getVersion(QString name)
{
    return shell->getOutput("dpkg-query -f '${Version}' -W " + name);
}

// return number of snapshots in snapshot_dir
int MainWindow::getSnapshotCount()
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    if (snapshot_dir.exists()) {
        QFileInfoList list = snapshot_dir.entryInfoList(QStringList("*.iso"), QDir::Files);
        return list.size();
    }
    return 0;
}

// return the size of the work folder
QString MainWindow::getSnapshotSize()
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    QString size;
    if (snapshot_dir.exists()) {
        QString cmd = QString("find \"%1\" -maxdepth 1 -type f -name '*.iso' -exec du -shc {} + | tail -1 | awk '{print $1}'").arg(snapshot_dir.absolutePath());
        size = shell->getOutput(cmd);
        if (size != "" ) {
            return size;
        }
    }
    return "0";
}

// List used space
void MainWindow::listUsedSpace()
{
    this->show();
    ui->buttonNext->setDisabled(true);
    ui->buttonCancel->setDisabled(true);
    ui->buttonSelectSnapshot->setDisabled(true);
    QString cmd;
    if (live) {
        cmd = QString("du --exclude-from=\"%1\" -sch / 2>/dev/null | tail -n1 | cut -f1").arg(snapshot_excludes.fileName());
    } else {
        cmd = QString("df -h / | awk 'NR==2 {print $3}'");
    }
    QString out = "\n- " + tr("Used space on / (root): ") + shell->getOutput(cmd);
    if (shell->run("mountpoint -q /home") == 0 ) {
        cmd = QString("df -h /home | awk 'NR==2 {print $3}'");
        out.append("\n- " + tr("Used space on /home: ") + shell->getOutput(cmd));
    }
    ui->buttonNext->setEnabled(true);
    ui->buttonCancel->setEnabled(true);
    ui->buttonSelectSnapshot->setEnabled(true);
    ui->labelUsedSpace->setText(out);
}


// List free space on drives
void MainWindow::listFreeSpace()
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    QString cmd;
    QString out;
    QString path = snapshot_dir.absolutePath().remove("/snapshot");
    cmd = QString("df -h \"%1\" | awk 'NR==2 {print $4}'").arg(path);
    ui->labelFreeSpace->clear();
    out.append("- " + tr("Free space on %1, where snapshot folder is placed: ").arg(path) + shell->getOutput(cmd) + "\n");
    ui->labelFreeSpace->setText(out);
    ui->labelDiskSpaceHelp->setText(tr("The free space should be sufficient to hold the compressed data from / and /home\n\n"
                                       "      If necessary, you can create more available space\n"
                                       "      by removing previous snapshots and saved copies:\n"
                                       "      %1 snapshots are taking up %2 of disk space.\n").arg(QString::number(getSnapshotCount())).arg(getSnapshotSize()));
}

// Checks if the editor listed in the config file is present
void MainWindow::checkEditor()
{
    if (gui_editor.exists()) {
        return;
    } else if (QFile("/usr/bin/featherpad").exists()) {
        gui_editor.setFileName("/usr/bin/featherpad");
        return;
    } else if (QFile("/usr/bin/leafpad").exists()) {
        gui_editor.setFileName("/usr/bin/leafpad");
        return;
    }
    this->show();
    QString msg = tr("The graphical text editor is set to %1, but it is not installed. Edit %2 "
                     "and set the gui_editor variable to the editor of your choice. "
                     "(examples: /usr/bin/gedit, /usr/bin/leafpad)\n\n"
                     "Will install leafpad and use it this time.").arg(gui_editor.fileName()).arg(config_file.fileName());
    QMessageBox::information(this, QString::null, msg);
    if (installPackage("leafpad")) {
        this->hide();
        gui_editor.setFileName("/usr/bin/leafpad");
    }
    ui->stackedWidget->setCurrentWidget(ui->settingsPage);
    ui->buttonNext->setEnabled(true);
    ui->buttonBack->setEnabled(true);
}

// Checks if package is installed
bool MainWindow::checkInstalled(QString package)
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    QString cmd = QString("dpkg -s %1 | grep Status").arg(package);
    if (shell->getOutput(cmd) == "Status: install ok installed") {
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
    shell->run("apt-get update");
    shell->run("apt-get install -y " + package);
    if (shell->getExitCode() != 0) {
        QMessageBox::critical(this, tr("Error"), tr("Could not install ") + package);
        disableOutput();
        return false;
    }
    disableOutput();
    return true;
}

void MainWindow::checkDirectories()
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    //  Create snapshot dir if it doesn't exist
    if (!snapshot_dir.exists()) {
        snapshot_dir.mkpath(snapshot_dir.absolutePath());
    }
    // Create a work_dir
    work_dir = shell->getOutput("mktemp -d \"" + snapshot_dir.absolutePath() + "/mx-snapshot-XXXXXXXX\"");
    system("mkdir -p " + work_dir.toUtf8() + "/iso-template/antiX");
    system("cd ..; cd -");
}

void MainWindow::openInitrd(QString file, QString initrd_dir)
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    ui->outputLabel->setText(tr("Building new initrd..."));
    QString cmd = "chmod a+rx \"" + initrd_dir + "\"";
    shell->run(cmd);
    QDir::setCurrent(initrd_dir);
    cmd = QString("gunzip -c \"%1\" | cpio -idum").arg(file);
    shell->run(cmd, QStringList() << "slowtick");
}

void MainWindow::closeInitrd(QString initrd_dir, QString file)
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    QDir::setCurrent(initrd_dir);
    QString cmd = "(find . | cpio -o -H newc --owner root:root | gzip -9) >\"" + file + "\"";
    shell->run(cmd, QStringList() << "slowtick");
    if (initrd_dir.startsWith("/tmp/tmp.")) {
        shell->run("rm -r " + initrd_dir);
    }
    makeMd5sum(work_dir + "/iso-template/antiX", "initrd.gz");
}

// Copying the iso-template filesystem
void MainWindow::copyNewIso()
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
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
        if(i686) {
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

    QString initrd_dir = shell->getOutput("mktemp -d");
    openInitrd(work_dir + "/iso-template/antiX/initrd.gz", initrd_dir);
    if (initrd_dir.startsWith("/tmp/tmp.")) {  //just make sure initrd_dir is correct to avoid disaster
        // strip modules
        shell->run("test -d \"" + initrd_dir + "/lib/modules\" && rm -r \"" + initrd_dir  + "/lib/modules\"");
    }
    shell->run("test -r /usr/local/share/live-files/files/etc/initrd-release && cp /usr/local/share/live-files/files/etc/initrd-release \"" + initrd_dir + "/etc\""); // We cannot count on this file in the future versions
    shell->run("test -r /etc/initrd-release && cp /etc/initrd-release \"" + initrd_dir + "/etc\""); // overwrite with this file, probably a better location _if_ the file exists
    if (initrd_dir != "") {
        copyModules(initrd_dir, kernel_used);
        closeInitrd(initrd_dir, work_dir + "/iso-template/antiX/initrd.gz");
    }
}

// replace text in menu items in grub.cfg, syslinux.cfg, isolinux.cfg
void MainWindow::replaceMenuStrings() {
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    QString date = shell->getOutput("date +'%d %B %Y'");
    QString distro = shell->getOutput("cat /etc/antix-version | cut -f1 -d'_'");
    QString distro_name = shell->getOutput("lsb_release -is");
    QString full_distro_name = shell->getOutput("cat /etc/antix-version | cut -f-2 -d' '");
    QString code_name = shell->getOutput("lsb_release -cs");
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
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    if (stamp == "datetime") {
        return snapshot_basename + "-" + shell->getOutput("date +%Y%m%d_%H%M") + ".iso";
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

// make working directory using the base filename
void MainWindow::mkDir(QString file_name)
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    QDir dir;
    QFileInfo fi(file_name);
    QString base_name = fi.completeBaseName(); // remove extension
    dir.setPath(work_dir + "/iso-template/" + base_name);
    dir.mkpath(dir.absolutePath());
}

// save package list in working directory
void MainWindow::savePackageList(QString file_name)
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    QFileInfo fi(file_name);
    QString base_name = fi.completeBaseName(); // remove extension
    QString full_name = work_dir + "/iso-template/" + base_name + "/package_list";
    QString cmd = "dpkg -l | grep ^ii\\ \\ | awk '{print $2,$3}' | sed 's/:'$(dpkg --print-architecture)'//' | column -t >\"" + full_name + "\"";
    shell->run(cmd);
}

// setup the environment before taking the snapshot
void MainWindow::setupEnv()
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    // checks if work_dir looks OK
    if (!work_dir.contains("/mx-snapshot")) {
        return;
    }

    // install mx-installer if absent
    if (force_installer && !checkInstalled("mx-installer")) {
        installPackage("mx-installer");
    }
    // setup environment if creating a respin (reset root/demo, remove personal accounts)
    if (reset_accounts) {
        shell->run("installed-to-live -b /.bind-root start empty=/home general version-file read-only");
    } else {
        if (force_installer == true) {  // copy minstall.desktop to Desktop on all accounts
            shell->run("echo /home/*/Desktop | xargs -n1 cp /usr/share/applications/minstall.desktop 2>/dev/null");
            shell->run("chmod +x /home/*/Desktop/minstall.desktop");
        }
        shell->run("installed-to-live -b /.bind-root start bind=/home live-files version-file adjtime read-only");
    }
}

int MainWindow::getDebianVersion()
{
    return shell->getOutput("cat /etc/debian_version | cut -f1 -d'.'").toInt();
}


// create squashfs and then the iso
bool MainWindow::createIso(QString filename)
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    // add exclusions snapshot dir
    addRemoveExclusion(true, snapshot_dir.absolutePath());

    if (reset_accounts) {
        // exclude /etc/localtime if link and timezone not America/New_York
        if (shell->run("test -L /etc/localtime") == 0 && shell->getOutput("cat /etc/timezone") != "America/New_York" ) {
            addRemoveExclusion(true, "/etc/localtime");
        }
    }

    // squash the filesystem copy
    QDir::setCurrent(work_dir);
    QString cmd;
    cmd = "mksquashfs /.bind-root iso-template/antiX/linuxfs " + mksq_opt + " -wildcards -ef " + snapshot_excludes.fileName() + " " + session_excludes;

    ui->outputLabel->setText(tr("Squashing filesystem..."));
    displayOutput();
    if (shell->run(cmd, QStringList() << "slowtick") != 0) {
        QMessageBox::critical(this, tr("Error"), tr("Could not create linuxfs file, please check whether you have enough space on the destination partition."));
        return false;
    }
    makeMd5sum(work_dir + "/iso-template/antiX", "linuxfs");

    // mv linuxfs to another folder
    system("mkdir -p iso-2/antiX");
    shell->run("mv iso-template/antiX/linuxfs* iso-2/antiX");

    // create the iso file
    QDir::setCurrent(work_dir + "/iso-template");
    cmd = "xorriso -as mkisofs -l -V MXLIVE -R -J -pad -iso-level 3 -no-emul-boot -boot-load-size 4 -boot-info-table -b boot/isolinux/isolinux.bin  -eltorito-alt-boot -e boot/grub/efi.img -no-emul-boot -c boot/isolinux/isolinux.cat -o \"" + snapshot_dir.absolutePath() + "/" + filename + "\" . \""  + work_dir + "/iso-2\"";
    ui->outputLabel->setText(tr("Creating CD/DVD image file..."));
    if (shell->run(cmd, QStringList() << "slowtick") != 0) {
        QMessageBox::critical(this, tr("Error"), tr("Could not create ISO file, please check whether you have enough space on the destination partition."));
        disableOutput();
        return false;
    }

    // make it isohybrid
    if (make_isohybrid == "yes") {
        ui->outputLabel->setText(tr("Making hybrid iso"));
        cmd = "isohybrid \"" + snapshot_dir.absolutePath() + "/" + filename + "\"";
        shell->run(cmd);
    }

    // make md5sum
    if (make_md5sum == "yes") {
        makeMd5sum(snapshot_dir.absolutePath(), filename);
    }
    disableOutput();
    return true;
}

// create md5sum for different files
void MainWindow::makeMd5sum(QString folder, QString file_name)
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    QDir dir;
    QString current = dir.currentPath();
    dir.setCurrent(folder);
    ui->outputLabel->setText(tr("Making md5sum"));
    QString cmd = "md5sum \"" + file_name + "\">\"" + folder + "/" + file_name + ".md5\"";
    shell->run(cmd, QStringList() << "slowtick");
    dir.setCurrent(current);
}

// clean up changes before exit
void MainWindow::cleanUp()
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    ui->outputLabel->setText(tr("Cleaning..."));
    shell->run("pkill mksquashfs; pkill md5sum");
    QDir::setCurrent("/");
    shell->run("installed-to-live cleanup");

    // checks if work_dir looks OK
    if (work_dir.contains("/mx-snapshot")) {
        shell->run("rm -r \"" + work_dir + "\"");
    }
    if (!live && !reset_accounts) {
        // remove installer icon
        shell->run("rm /home/*/Desktop/minstall.desktop");
    }
    ui->outputLabel->setText(tr("Done"));
}

// adds or removes exclusion to the exclusion string
void MainWindow::addRemoveExclusion(bool add, QString exclusion)
{
    qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    if (exclusion.startsWith("/")) {
        exclusion.remove(0, 1); // remove training slash
    }
    if (add) {
        if (session_excludes == "") {
            session_excludes.append("-e '" + exclusion + "'");
        } else {
            session_excludes.append(" '" + exclusion + "'");
        }
    } else {
        session_excludes.remove(" '" + exclusion + "'");
        if (session_excludes == "-e") {
            session_excludes = "";
        }
    }
}

void MainWindow::displayDoc(QString url)
{
    QProcess proc;
    proc.start("logname");
    proc.waitForFinished();
    QString user = proc.readAllStandardOutput().trimmed();
    QString exec = "xdg-open";
    if (shell->run("command -v mx-viewer") == 0) { // use mx-viewer if available
        exec = "mx-viewer";
    }
    QString cmd = "su " + user + " -c \"" + exec + " " + url + "\"&";
    shell->run(cmd);
}

//// sync process events ////
void MainWindow::procStart()
{
    setCursor(QCursor(Qt::BusyCursor));
}

void MainWindow::procDone()
{
    ui->progressBar->setValue(ui->progressBar->maximum());
    setCursor(QCursor(Qt::ArrowCursor));
}

// set proc and timer connections
void MainWindow::displayOutput()
{
    connect(shell, &Cmd::outputAvailable, this, &MainWindow::outputAvailable);
    connect(shell, &Cmd::errorAvailable, this, &MainWindow::outputAvailable);
}

void MainWindow::disableOutput()
{
    disconnect(shell, &Cmd::outputAvailable, 0, 0);
    disconnect(shell, &Cmd::errorAvailable, 0, 0);
}

// update output box on Stdout
void MainWindow::outputAvailable(const QString &output)
{
    ui->outputBox->insertPlainText(output);
    QScrollBar *sb = ui->outputBox->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void MainWindow::progress(int counter, int duration) // processes tick emited by Cmd to be used by a progress bar
{
    ui->progressBar->setMaximum(duration);
    ui->progressBar->setValue(counter % (duration + 1));
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
        if (edit_boot_menu == "yes") {
            checkEditor();
        }
        kernel_used = shell->getOutput("uname -r");
        ui->stackedWidget->setCurrentWidget(ui->settingsPage);
        ui->label_1->setText(tr("Snapshot will use the following settings:*"));

        ui->label_2->setText("\n" + tr("- Snapshot directory:") + " " + snapshot_dir.absolutePath() + "\n" +
                       "- " + tr("Snapshot name:") + " " + file_name + "\n" +
                       tr("- Kernel to be used:") + " " + kernel_used + "\n");
        ui->label_3->setText(tr("*These settings can be changed by editing: ") + config_file.fileName());

    // on settings page
    } else if (ui->stackedWidget->currentWidget() == ui->settingsPage) {

        int ans = QMessageBox::question(this, tr("Final chance"),
                              tr("Snapshot now has all the information it needs to create an ISO from your running system.") + "\n\n" +
                              tr("It will take some time to finish, depending on the size of the installed system and the capacity of your computer.") + "\n\n" +
                              tr("OK to start?"), QMessageBox::Ok | QMessageBox::Cancel);
        if (ans == QMessageBox::Cancel) {
            return;
        }
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
                QString cmd = gui_editor.fileName() + " \"" + work_dir + "/iso-template/boot/isolinux/isolinux.cfg\"";
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

void MainWindow::on_buttonEditConfig_clicked()
{
    this->hide();
    checkEditor();
    shell->run((gui_editor.fileName() + " " + config_file.fileName()));
    setup();
}

void MainWindow::on_buttonEditExclude_clicked()
{
    this->hide();
    checkEditor();
    shell->run((gui_editor.fileName() + " " + snapshot_excludes.fileName()));
    this->show();
}

void MainWindow::on_excludeDocuments_toggled(bool checked)
{
    QString user = shell->getOutput("logname");
    QString xdg_user_dir = shell->getOutput("su " + user + " -c \"xdg-user-dir DOCUMENTS\"") + "/*";
    xdg_user_dir.replace(user, "*");
    if (xdg_user_dir.startsWith("/")) {
        xdg_user_dir.remove(0, 1); // remove training slash
    }
    QString exclusion = "/home/*/Documents/*' '" + xdg_user_dir;
    addRemoveExclusion(checked, exclusion);
    if (!checked) {
        ui->excludeAll->setChecked(false);
    }
}

void MainWindow::on_excludeDownloads_toggled(bool checked)
{
    QString user = shell->getOutput("logname");
    QString xdg_user_dir = shell->getOutput("su " + user + " -c \"xdg-user-dir DOWNLOAD\"") + "/*";
    xdg_user_dir.replace(user, "*");
    if (xdg_user_dir.startsWith("/")) {
        xdg_user_dir.remove(0, 1); // remove training slash
    }
    QString exclusion = "/home/*/Downloads/*' '" + xdg_user_dir;
    addRemoveExclusion(checked, exclusion);
    if (!checked) {
        ui->excludeAll->setChecked(false);
    }
}

void MainWindow::on_excludePictures_toggled(bool checked)
{
    QString user = shell->getOutput("logname");
    QString xdg_user_dir = shell->getOutput("su " + user + " -c \"xdg-user-dir PICTURES\"") + "/*";
    xdg_user_dir.replace(user, "*");
    if (xdg_user_dir.startsWith("/")) {
        xdg_user_dir.remove(0, 1); // remove training slash
    }
    QString exclusion = "/home/*/Pictures/*' '" + xdg_user_dir;
    addRemoveExclusion(checked, exclusion);
    if (!checked) {
        ui->excludeAll->setChecked(false);
    }
}

void MainWindow::on_excludeMusic_toggled(bool checked)
{
    QString user = shell->getOutput("logname");
    QString xdg_user_dir = shell->getOutput("su " + user + " -c \"xdg-user-dir MUSIC\"") + "/*";
    xdg_user_dir.replace(user, "*");
    if (xdg_user_dir.startsWith("/")) {
        xdg_user_dir.remove(0, 1); // remove training slash
    }
    QString exclusion = "/home/*/Music/*' '" + xdg_user_dir;
    addRemoveExclusion(checked, exclusion);
    if (!checked) {
        ui->excludeAll->setChecked(false);
    }
}

void MainWindow::on_excludeVideos_toggled(bool checked)
{
    QString user = shell->getOutput("logname");
    QString xdg_user_dir = shell->getOutput("su " + user + " -c \"xdg-user-dir VIDEOS\"") + "/*";
    xdg_user_dir.replace(user, "*");
    if (xdg_user_dir.startsWith("/")) {
        xdg_user_dir.remove(0, 1); // remove training slash
    }
    QString exclusion = "/home/*/Videos/*' '" + xdg_user_dir;
    addRemoveExclusion(checked, exclusion);
    if (!checked) {
        ui->excludeAll->setChecked(false);
    }
}

void MainWindow::on_excludeDesktop_toggled(bool checked)
{
    QString user = shell->getOutput("logname");
    QString xdg_user_dir = shell->getOutput("su " + user + " -c \"xdg-user-dir DESKTOP\"");
    xdg_user_dir.replace(user, "*");
    if (xdg_user_dir.startsWith("/")) {
        xdg_user_dir.remove(0, 1); // remove training slash
    }
    QString exclusion = "/home/*/Desktop/!(minstall.desktop)' '" + xdg_user_dir + "/!(minstall.desktop)";
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
    QMessageBox msgBox(QMessageBox::NoIcon,
                       tr("About MX Snapshot"), "<p align=\"center\"><b><h2>" +
                       tr("MX Snapshot") + "</h2></b></p><p align=\"center\">" + tr("Version: ") +
                       version + "</p><p align=\"center\"><h3>" +
                       tr("Program for creating a live-CD from the running system for MX Linux") + "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p><p align=\"center\">" +
                       tr("Copyright (c) MX Linux") + "<br /><br /></p>", 0, this);
    QPushButton *btnLicense = msgBox.addButton(tr("License"), QMessageBox::HelpRole);
    QPushButton *btnChangelog = msgBox.addButton(tr("Changelog"), QMessageBox::HelpRole);
    QPushButton *btnCancel = msgBox.addButton(tr("Cancel"), QMessageBox::NoRole);
    btnCancel->setIcon(QIcon::fromTheme("window-close"));

    msgBox.exec();

    if (msgBox.clickedButton() == btnLicense) {
        QString url = "file:///usr/share/doc/mx-snapshot/license.html";
        displayDoc(url);
    } else if (msgBox.clickedButton() == btnChangelog) {
        QDialog *changelog = new QDialog(this);
        changelog->resize(600, 500);

        QTextEdit *text = new QTextEdit;
        text->setReadOnly(true);
        Cmd cmd;
        text->setText(cmd.getOutput("zless /usr/share/doc/" + QFileInfo(QCoreApplication::applicationFilePath()).fileName()  + "/changelog.gz"));

        QPushButton *btnClose = new QPushButton(tr("&Close"));
        btnClose->setIcon(QIcon::fromTheme("window-close"));
        connect(btnClose, &QPushButton::clicked, changelog, &QDialog::close);

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(text);
        layout->addWidget(btnClose);
        changelog->setLayout(layout);
        changelog->exec();
    }
    this->show();
}

// Help button clicked
void MainWindow::on_buttonHelp_clicked()
{
    QLocale locale;
    QString lang = locale.bcp47Name();

    QString url = "https://mxlinux.org/wiki/help-files/help-mx-save-system-iso-snapshot";

    if (lang.startsWith("fr")) {
        url = "https://mxlinux.org/wiki/help-files/help-mx-instantan%C3%A9";
    }
    displayDoc(url);
}

// Select snapshot directory
void MainWindow::on_buttonSelectSnapshot_clicked()
{
    QFileDialog dialog;

    QString selected = dialog.getExistingDirectory(this, tr("Select Snapshot Directory"), QString(), QFileDialog::ShowDirsOnly);
    if (selected != "") {
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