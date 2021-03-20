/**********************************************************************
 *  work.cpp
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

#include <QDebug>
#include <QDate>
#include <QSettings>

#include "work.h"

Work::Work(Settings *settings) :
    settings(settings)
{
    if (!settings->checkCompression()) {
        emit messageBox(BoxType::critical, tr("Error"), tr("Current kernel doesn't support selected compression algorithm, please edit the configuration file and select a different algorithm."));
        cleanUp();
    }
}

Work::~Work()
{

}

// Checks if there's enough space on partitions, if not post error, cleanup and exit
// We don't yet take /home used space into considerations (need to calculate how much is excluded)
void Work::checkEnoughSpace()
{
    uint c_factor = compression_factor.value(settings->compression);
    quint64 adjusted_root = settings->root_size * c_factor / 100;
    // if snapshot and workdir are on the same partition we need about double the size of adjusted_root
    if (settings->shell->getCmdOut("stat -c '%d' " + settings->work_dir) ==
            settings->shell->getCmdOut("stat -c '%d' " + settings->snapshot_dir)) {
        if (settings->free_space < adjusted_root * 2) {
            emit messageBox(BoxType::critical, tr("Error"),
                            tr("There's not enough free space on your target disk, you need at least %1").arg(QString::number(adjusted_root * 2 / 1048576.0, 'f', 2) + "GiB" ) + "\n" +
                            tr("You have %1 free space on %2").arg(QString::number(settings->free_space / 1048576.0, 'f', 2) + "GiB").arg(settings->snapshot_dir));
            cleanUp();
        }
    } else { // if not on the same partitions, check if each work_dir and snapshot_dir partitions have enough free space for 1 adjusted_root
        if (settings->free_space_work < adjusted_root) {
            emit messageBox(BoxType::critical, tr("Error"),
                            tr("There's not enough free space on your target disk, you need at least %1").arg(QString::number(adjusted_root * 2 / 1048576.0, 'f', 2) + "GiB") + "\n" +
                            tr("You have %1 free space on %2").arg(QString::number(settings->free_space_work / 1048576.0, 'f', 2) + "GiB").arg(settings->work_dir));
            cleanUp();
        }
        if (settings->free_space < adjusted_root) {
            emit messageBox(BoxType::critical, tr("Error"),
                            tr("There's not enough free space on your target disk, you need at least %1").arg(QString::number(adjusted_root * 2 / 1048576.0, 'f', 2) + "GiB") + "\n" +
                            tr("You have %1 free space on %2").arg(QString::number(settings->free_space / 1048576.0, 'f', 2) + "GiB").arg(settings->snapshot_dir));
            cleanUp();
        }
    }
}

// Checks if package is installed
bool Work::checkInstalled(const QString &package)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QString cmd = QString("dpkg -s %1 | grep Status").arg(package);
    if (settings->shell->getCmdOut(cmd) == "Status: install ok installed")
        return true;
    return false;
}

// clean up changes before exit
void Work::cleanUp()
{
    if (not started ) {
        settings->shell->halt();
        initrd_dir.remove();
        exit(EXIT_SUCCESS);
    }
    settings->shell->halt();
    emit message(tr("Cleaning..."));
    system("sync");

    system("pkill mksquashfs; pkill md5sum");
    QDir::setCurrent("/");
    if (QFileInfo::exists("/tmp/installed-to-live/cleanup.conf"))
        system("installed-to-live cleanup");

    if (!settings->live && !settings->reset_accounts)
        system("rm /home/*/Desktop/minstall.desktop 2>/dev/null");

    if (!settings->live) QFile::remove("/etc/skel/Desktop/Installer.desktop");

    initrd_dir.remove();
    settings->tmpdir.reset();
    if (done) {
        qDebug().noquote() << tr("Done");
        exit(EXIT_SUCCESS);
    } else {
        qDebug().noquote() << tr("Done") << endl;
        qDebug().noquote() << QObject::tr("Interrupted or failed to complete");
        exit(EXIT_FAILURE);
    }
}

void Work::closeInitrd(const QString &initrd_dir, const QString &file)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QDir::setCurrent(initrd_dir);
    QString cmd = "(find . | cpio -o -H newc --owner root:root | gzip -9) >\"" + file + "\"";
    settings->shell->run(cmd);
    makeChecksum(HashType::md5, settings->work_dir + "/iso-template/antiX", "initrd.gz");
}


// copyModules(mod_dir/kernel kernel)
void Work::copyModules(const QString &to, const QString &kernel)
{
    QString kernel586 = "3.16.0-4-586";
    QString cmd = QString("/usr/share/mx-packageinstaller/scripts/copy-initrd-modules -t=\"%1\" -k=\"%2\"").arg(to).arg(kernel);
    settings->shell->run(cmd);
    // copy 586 modules for the non-PAE kernel
    if (settings->i686 && settings->debian_version < 9) {  // Not applicable for Stretch (MX17) or more
        QString cmd = QString("/usr/share/mx-packageinstaller/scripts/copy-initrd-modules -t=\"%1\" -k=\"%2\"").arg(to).arg(kernel586);
        settings->shell->run(cmd);
    }
    cmd = QString("/usr/share/mx-packageinstaller/scripts/copy-initrd-programs --to=\"%1\"").arg(to);
    settings->shell->run(cmd);
}


// Copying the iso-template filesystem
void Work::copyNewIso()
{
    emit message(tr("Copying the new-iso filesystem..."));
    QDir::setCurrent(settings->work_dir);

    QString cmd = "tar xf /usr/lib/iso-template/iso-template.tar.gz";
    settings->shell->run(cmd);

    cmd = "cp /usr/lib/iso-template/template-initrd.gz iso-template/antiX/initrd.gz";
    settings->shell->run(cmd);

    cmd = "cp /boot/vmlinuz-" + settings->kernel + " iso-template/antiX/vmlinuz";
    settings->shell->run(cmd);

    if (settings->debian_version < 9) { // Only for versions older than Stretch
        if (settings->i686) {
            settings->shell->run("cp /boot/vmlinuz-3.16.0-4-586 iso-template/antiX/vmlinuz1");
        } else {
            // mv x64 template files over
            settings->shell->run("mv iso-template/boot/grub/grub.cfg_x64 iso-template/boot/grub/grub.cfg");
            settings->shell->run("mv iso-template/boot/syslinux/syslinux.cfg_x64 iso-template/boot/syslinux/syslinux.cfg");
            settings->shell->run("mv iso-template/boot/isolinux/isolinux.cfg_x64 iso-template/boot/isolinux/isolinux.cfg");
        }
    }

    replaceMenuStrings();
    makeChecksum(HashType::md5, settings->work_dir + "/iso-template/antiX", "vmlinuz");

    QString path = initrd_dir.path();
    if(!initrd_dir.isValid()) {
        qDebug() << tr("Could not create temp directory. ") + path;
        cleanUp();
    }

    openInitrd(settings->work_dir + "/iso-template/antiX/initrd.gz", path);

    // strip modules; make sure initrd_dir is correct to avoid disaster
    if (path.startsWith("/tmp/") && QFileInfo::exists(path + "/lib/modules"))
         settings->shell->run("rm -r \"" + path  + "/lib/modules\"");

    settings->shell->run("test -r /usr/local/share/live-files/files/etc/initrd-release && cp /usr/local/share/live-files/files/etc/initrd-release \"" + path + "/etc\""); // We cannot count on this file in the future versions
    settings->shell->run("test -r /etc/initrd-release && cp /etc/initrd-release \"" + path + "/etc\""); // overwrite with this file, probably a better location _if_ the file exists
    if (initrd_dir.isValid()) {
        copyModules(path, settings->kernel);
        closeInitrd(path, settings->work_dir + "/iso-template/antiX/initrd.gz");
        initrd_dir.remove();
    }
}

// create squashfs and then the iso
bool Work::createIso(const QString &filename)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    // squash the filesystem copy
    QDir::setCurrent(settings->work_dir);
    QString cmd;
    cmd = "mksquashfs /.bind-root iso-template/antiX/linuxfs -comp " + settings->compression + ((settings->mksq_opt.isEmpty()) ? "" : " " + settings->mksq_opt)
            + " -wildcards -ef " + settings->snapshot_excludes.fileName() + " " + settings->session_excludes;

    emit message(tr("Squashing filesystem..."));
    if (!settings->shell->run(cmd)) {
        emit messageBox(BoxType::critical, tr("Error"), tr("Could not create linuxfs file, please check whether you have enough space on the destination partition."));
        return false;
    }

    // mv linuxfs to another folder
    QDir().mkpath("iso-2/antiX");
    settings->shell->run("mv iso-template/antiX/linuxfs* iso-2/antiX");
    makeChecksum(HashType::md5, settings->work_dir + "/iso-2/antiX", "linuxfs");

    settings->shell->run("installed-to-live cleanup");

    // create the iso file
    QDir::setCurrent(settings->work_dir + "/iso-template");
    cmd = "xorriso -as mkisofs -l -V MXLIVE -R -J -pad -iso-level 3 -no-emul-boot -boot-load-size 4 -boot-info-table -b boot/isolinux/isolinux.bin  -eltorito-alt-boot -e boot/grub/efi.img -no-emul-boot -c boot/isolinux/isolinux.cat -o \"" +
            settings->snapshot_dir + "/" + filename + "\" . \""  + settings->work_dir + "/iso-2\"";
    emit message(tr("Creating CD/DVD image file..."));
    if (!settings->shell->run(cmd)) {
        emit messageBox(BoxType::critical, tr("Error"), tr("Could not create ISO file, please check whether you have enough space on the destination partition."));
        return false;
    }
    system("chown $(logname):$(logname) \"" + settings->snapshot_dir.toUtf8() + "/" + filename.toUtf8( )+ "\"");

    // make it isohybrid
    if (settings->make_isohybrid) {
        emit message(tr("Making hybrid iso"));
        cmd = "isohybrid --uefi \"" + settings->snapshot_dir + "/" + filename + "\"";
        settings->shell->run(cmd);
    }

    // make ISO checksums
    if (settings->make_chksum) {
        makeChecksum(HashType::md5, settings->snapshot_dir, filename);
        makeChecksum(HashType::sha512, settings->snapshot_dir, filename);
        system("chown $(logname):$(logname) \"" + settings->snapshot_dir.toUtf8() + "/" + filename.toUtf8() + ".md5\"");
        system("chown $(logname):$(logname) \"" + settings->snapshot_dir.toUtf8() + "/" + filename.toUtf8() + ".sha512\"");
    }

    QTime time(0, 0);
    time = time.addMSecs(static_cast<int>(e_timer.elapsed()));
    emit message(tr("Done"));
    emit messageBox(BoxType::information, tr("Success"), tr("MX Snapshot completed sucessfully!") + "\n" +
                    tr("Snapshot took %1 to finish.").arg(time.toString("hh:mm:ss")) + "\n\n" +
                    tr("Thanks for using MX Snapshot, run MX Live USB Maker next!"));
    done = true;
    return true;
}


// Installs package
bool Work::installPackage(const QString &package)
{
    emit message(tr("Installing ") + package);
    settings->shell->run("apt-get update");
    if (!settings->shell->run("apt-get install -y " + package)) {
        emit messageBox(BoxType::critical, tr("Error"), tr("Could not install ") + package);
        return false;
    }
    return true;
}

// create checksums for different files
void Work::makeChecksum(const HashType &hash_type, const QString &folder, const QString &file_name)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    emit message(tr("Calculating checksum..."));
    settings->shell->run("sync");
    QDir::setCurrent(folder);
    QString ce = QVariant::fromValue(hash_type).toString();
    QString cmd;
    QString checksum_cmd =  QString("%1sum \"" + file_name + "\">\"" + folder + "/" + file_name + ".%1\"").arg(ce);
    QString temp_dir =  "/tmp/snapsphot-checksum-temp";
    QString checksum_tmp =  QString("TD=" + temp_dir + "; KEEP=$TD/.keep; [ -d $TD ] || mkdir $TD ; FN=\"" + file_name + "\"; CF=\""
            + folder + "/${FN}.%1\"; cp $FN $TD/$FN; pushd $TD>/dev/null; %1sum $FN > $FN.%1 ; cp $FN.%1 $CF; popd >/dev/null ; [ -e $KEEP ] || rm -rf $TD").arg(ce);

    if (settings->preempt) {
        // check free space available on /tmp
        settings->shell->run("TF=/tmp/snapsphot-checksum-temp/\"" + file_name + "\"; [ -f \"$TF\" ] && rm -f \"$TF\"");
        if (!settings->shell->run("DUF=$(du -BM " + file_name + "| grep -oE '^[[:digit:]]+'); TDA=$(df -BM --output=avail /tmp | grep -oE '^[[:digit:]]+'); ((TDA/10*8 >= DUF))"))
            settings->preempt = false;
    }
    if (!settings->preempt) {
        cmd = checksum_cmd;
    } else {
        // free pagecache
        settings->shell->run("sync; sleep 1; echo 1 > /proc/sys/vm/drop_caches; sleep 1");
        cmd = checksum_tmp;
    }
    settings->shell->run(cmd);
    QDir::setCurrent(settings->work_dir);
}

// make working directory using the base filename
bool Work::mkDir(const QString &file_name)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QDir dir;
    QFileInfo fi(file_name);
    dir.setPath(settings->work_dir + "/iso-template/" + fi.completeBaseName());
    if (!dir.mkpath(dir.absolutePath())) {
        emit messageBox(BoxType::critical, tr("Error"), tr("Could not create working directory. ") + dir.absolutePath());
        return false;
    }
    return true;
}

void Work::openInitrd(const QString &file, const QString &initrd_dir)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    emit message(tr("Building new initrd..."));
    QString cmd = "chmod a+rx \"" + initrd_dir + "\"";
    settings->shell->run(cmd);
    QDir::setCurrent(initrd_dir);
    cmd = QString("gunzip -c \"%1\" | cpio -idum").arg(file);
    settings->shell->run(cmd);
}

// replace text in menu items in grub.cfg, syslinux.cfg, isolinux.cfg
void Work::replaceMenuStrings() {
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";

    QString distro, full_distro_name;
    if (QFileInfo::exists("/etc/antix-version")) {
        distro = settings->shell->getCmdOut("cat /etc/antix-version | cut -f1 -d'_'");
        full_distro_name = settings->shell->getCmdOut("cat /etc/antix-version | cut -f-2 -d' '");
    } else {
        distro = "MX_" + QString(settings->i686 ? "386" : "x64");
        full_distro_name = distro;
    }
    QString date = QDate::currentDate().toString("dd MMMM yyyy");
    if (not QFileInfo::exists("/etc/lsb-release")) {
        emit messageBox(BoxType::critical, tr("Error"), tr("Could not find %1 file, cannot continue").arg("/etc/lsb-release"));
        cleanUp();
    }
    QString distro_name = settings->shell->getCmdOut("grep -oP '(?<=DISTRIB_ID=).*' /etc/lsb-release");
    QString code_name = settings->shell->getCmdOut("grep -oP '(?<=DISTRIB_CODENAME=).*' /etc/lsb-release");
    QString options = "quiet";

    if (settings->debian_version < 9) { // Only for versions older than Stretch which uses old mx-iso-template
        if (settings->i686) {
            QString new_string = "MX Linux 386 (" + date + ")";
            replaceStringInFile("custom-name", new_string, settings->work_dir + "/iso-template/boot/grub/grub.cfg");
            replaceStringInFile("custom-name", new_string, settings->work_dir + "/iso-template/boot/syslinux/syslinux.cfg");
            replaceStringInFile("custom-name", new_string, settings->work_dir + "/iso-template/boot/isolinux/isolinux.cfg");
        } else {
            QString new_string = "MX Linux x64 (" + date + ")";
            replaceStringInFile("custom-name", new_string, settings->work_dir + "/iso-template/boot/grub/grub.cfg");
            replaceStringInFile("custom-name", new_string, settings->work_dir + "/iso-template/boot/syslinux/syslinux.cfg");
            replaceStringInFile("custom-name", new_string, settings->work_dir + "/iso-template/boot/isolinux/isolinux.cfg");
        }

    } else { // with new mx-iso-template for MX-17 and greater
        replaceStringInFile("%DISTRO_NAME%", distro_name, settings->work_dir + "/iso-template/boot/grub/grub.cfg");

        replaceStringInFile("%OPTIONS%", options, settings->work_dir + "/iso-template/boot/syslinux/syslinux.cfg");
        replaceStringInFile("%OPTIONS%", options, settings->work_dir + "/iso-template/boot/isolinux/isolinux.cfg");

        replaceStringInFile("%FULL_DISTRO_NAME%", full_distro_name, settings->work_dir + "/iso-template/boot/grub/grub.cfg");
        replaceStringInFile("%FULL_DISTRO_NAME%", full_distro_name, settings->work_dir + "/iso-template/boot/syslinux/syslinux.cfg");
        replaceStringInFile("%FULL_DISTRO_NAME%", full_distro_name, settings->work_dir + "/iso-template/boot/syslinux/readme.msg");
        replaceStringInFile("%FULL_DISTRO_NAME%", full_distro_name, settings->work_dir + "/iso-template/boot/isolinux/isolinux.cfg");
        replaceStringInFile("%FULL_DISTRO_NAME%", full_distro_name, settings->work_dir + "/iso-template/boot/isolinux/readme.msg");

        replaceStringInFile("%DISTRO%", distro, settings->work_dir + "/iso-template/boot/grub/theme/theme.txt");
        replaceStringInFile("%DISTRO%", distro, settings->work_dir + "/iso-template/boot/grub/grub.cfg");

        replaceStringInFile("%RELEASE_DATE%", date, settings->work_dir + "/iso-template/boot/grub/grub.cfg");
        replaceStringInFile("%RELEASE_DATE%", date, settings->work_dir + "/iso-template/boot/syslinux/syslinux.cfg");
        replaceStringInFile("%RELEASE_DATE%", date, settings->work_dir + "/iso-template/boot/syslinux/readme.msg");
        replaceStringInFile("%RELEASE_DATE%", date, settings->work_dir + "/iso-template/boot/isolinux/isolinux.cfg");
        replaceStringInFile("%RELEASE_DATE%", date, settings->work_dir + "/iso-template/boot/isolinux/readme.msg");

        replaceStringInFile("%CODE_NAME%", code_name, settings->work_dir + "/iso-template/boot/syslinux/syslinux.cfg");
        replaceStringInFile("%CODE_NAME%", code_name, settings->work_dir + "/iso-template/boot/isolinux/isolinux.cfg");
        replaceStringInFile("%ASCII_CODE_NAME%", code_name, settings->work_dir + "/iso-template/boot/grub/theme/theme.txt");
    }
}

// Util function for replacing strings in files
bool Work::replaceStringInFile(const QString &old_text, const QString &new_text, const QString &file_path)
{
    QString cmd = QString("sed -i 's/%1/%2/g' \"%3\"").arg(old_text).arg(new_text).arg(file_path);
    return settings->shell->run(cmd);
}

// save package list in working directory
void Work::savePackageList(const QString &file_name)
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    QFileInfo fi(file_name);
    QString full_name = settings->work_dir + "/iso-template/" + fi.completeBaseName() + "/package_list";
    QString cmd = "dpkg -l | grep ^ii\\ \\ | awk '{print $2,$3}' | sed 's/:'$(dpkg --print-architecture)'//' | column -t >\"" + full_name + "\"";
    settings->shell->run(cmd);
}

// setup the environment before taking the snapshot
void Work::setupEnv()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    // checks if work_dir looks OK
    if (!settings->work_dir.contains("/mx-snapshot"))
        cleanUp();

    QString bind_boot = "";
    QString bind_boot_too = "";
    if (settings->shell->run("mountpoint /boot")) {
        bind_boot = "bind=/boot ";
        bind_boot_too = ",/boot";
    }

    // install mx-installer if absent
    if (settings->force_installer && !checkInstalled("mx-installer"))
        installPackage("mx-installer");

    writeSnapshotInfo();

    // setup environment if creating a respin (reset root/demo, remove personal accounts)
    if (settings->reset_accounts) {
        settings->shell->run("installed-to-live -b /.bind-root start " + bind_boot + "empty=/home general version-file read-only");
    } else {
        if (settings->force_installer) {  // copy minstall.desktop to Desktop on all accounts
            settings->shell->run("echo /home/*/Desktop | xargs -n1 cp /usr/share/applications/minstall.desktop 2>/dev/null");
            settings->shell->run("chmod 777 /home/*/Desktop/minstall.desktop"); // needs write access to remove lock symbol on installer on desktop, executable to run it
            if (not QFile::exists("xdg-user-dirs-update.real")) {
                if (not QFile::exists("/etc/skel/Desktop")) QDir().mkdir("/etc/skel/Desktop");
                QFile::copy("/usr/share/applications/minstall.desktop", "/etc/skel/Desktop/Installer.desktop");
                settings->shell->run("chmod 755 /etc/skel/Desktop/Installer.desktop");
            }
        }
        settings->shell->run("installed-to-live -b /.bind-root start bind=/home" + bind_boot_too + " live-files version-file adjtime read-only");
    }
}

// write date of the snapshot in a "snapshot_created" file
void Work::writeSnapshotInfo()
{
    QFile file("/usr/local/share/live-files/files/etc/snapshot_created");
    if (!file.open(QFile::WriteOnly | QFile::Truncate))
        return;

    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString("yyyyMMdd_HHmm");
    file.close();
}
