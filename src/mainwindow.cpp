/**********************************************************************
 *  mainwindow.cpp
 **********************************************************************
 * Copyright (C) 2015-2026 MX Authors
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

#include <QCalendarWidget>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QTextStream>
#include <QTime>
#include <unistd.h>

#include "about.h"
#include "excludesutils.h"
#include "settings.h"
#include "squashfsutils.h"
#include "terminalcursor.h"
#include "unitsecrets.h"
#include "work.h"
#include <chrono>

using namespace std::chrono_literals;

MainWindow::MainWindow(Settings *settings, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::MainWindow),
      settings(settings),
      work(settings, this)
{
    ui->setupUi(this);
    setConnections();
    setup();
    restoreGeometry(qt_settings.value("geometry").toByteArray());
    loadSettings();
    listFreeSpace();
    setExclusions();
    setOtherOptions();
    if (settings->monthly) {
        // Run inside exec(): Work::cleanUp() ends with QCoreApplication::exit(),
        // which exec() discards if it is requested before the loop starts.
        QTimer::singleShot(0, this, [this] {
            ui->btnNext->click();
            ui->btnNext->click();
        });
    } else {
        listUsedSpace();
    }
    watchExcludesFile();
}

MainWindow::~MainWindow() = default;

void MainWindow::loadSettings()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    ui->labelTitleSummary->clear();
    ui->labelSummary->clear();
    ui->labelSnapshotDir->setText(settings->snapshotDir);
    if (settings->snapshotName.isEmpty()) {
        ui->lineEditName->setText(settings->getFilename());
    } else {
        ui->lineEditName->setText(settings->snapshotName);
    }
    ui->textCodename->setText(settings->codename);
    ui->textDistroVersion->setText(settings->distroVersion);
    ui->textProjectName->setText(settings->projectName);
    ui->textOptions->setText(settings->bootOptions);
    ui->pushReleaseDate->setText(settings->releaseDate);
    ui->checkShutdownOutput->setChecked(settings->shutdown);
    QDir bootDir("/boot");
    QStringList kernelFiles = bootDir.entryList({"vmlinuz-*"}, QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    std::transform(kernelFiles.begin(), kernelFiles.end(), kernelFiles.begin(),
                   [](const QString &file) { return file.mid(QStringLiteral("vmlinuz-").length()); });
    ui->comboLiveKernel->addItems(kernelFiles);
    ui->comboLiveKernel->setCurrentText(settings->kernel);
    updateCustomExcludesButton();
    watchExcludesFile();
}

bool MainWindow::hasCustomExcludes() const
{
    const QString configuredPath = settings->snapshotExcludes.fileName();
    const QString sourcePath = settings->getExcludesSourcePath();

    if (sourcePath.isEmpty() || configuredPath.isEmpty()) {
        return false;
    }

    if (!QFileInfo::exists(sourcePath)) {
        return false;
    }

    if (!QFileInfo::exists(configuredPath)) {
        return true;
    }

    const int diffResult = QProcess::execute("diff", {"--brief", configuredPath, sourcePath});
    if (diffResult == 1) {
        return true;
    }
    if (diffResult != 0) {
        qWarning() << "Unable to compare excludes files with diff:" << configuredPath << sourcePath;
    }
    return false;
}

bool MainWindow::isSourceExcludesNewer(QString &diffOutput) const
{
    return ExcludesUtils::isSourceExcludesNewer(settings->snapshotExcludes.fileName(),
                                                settings->getExcludesSourcePath(), diffOutput);
}

void MainWindow::updateCustomExcludesButton()
{
    ui->btnRemoveCustomExclude->setVisible(hasCustomExcludes());
}

void MainWindow::watchExcludesFile()
{
    const QString path = settings->snapshotExcludes.fileName();
    if (path.isEmpty()) {
        return;
    }

    const QStringList watched = excludesWatcher.files();
    for (const QString &existing : watched) {
        if (existing != path) {
            excludesWatcher.removePath(existing);
        }
    }

    if (!excludesWatcher.files().contains(path) && QFileInfo::exists(path)) {
        excludesWatcher.addPath(path);
    }
}

MainWindow::ExcludesChoice MainWindow::showUpdatedExcludesPrompt(const QString &configuredPath,
                                                                 const QString &sourcePath) const
{
    QMessageBox msgBox(const_cast<MainWindow *>(this));
    msgBox.setWindowTitle(tr("Updated Exclusion List"));
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText(
        tr("The exclusion file at %1 is newer than your configured file at %2.").arg(sourcePath, configuredPath));
    msgBox.setInformativeText(
        tr("Review the changes below. Keep your custom file or replace it with the updated default."));

    ExcludesChoice choice = ExcludesChoice::None;

    msgBox.addButton(QMessageBox::Close);

    QPushButton *showDiffButton = msgBox.addButton(tr("Show Differences"), QMessageBox::ActionRole);
    QObject::connect(showDiffButton, &QPushButton::clicked, &msgBox, [&choice, &msgBox] {
        choice = ExcludesChoice::ShowDiff;
        msgBox.accept();
    });

    QPushButton *keepCustomButton = msgBox.addButton(tr("Keep Custom"), QMessageBox::ActionRole);
    QObject::connect(keepCustomButton, &QPushButton::clicked, &msgBox, [&choice, &msgBox] {
        choice = ExcludesChoice::KeepCustom;
        msgBox.accept();
    });

    QPushButton *useUpdatedDefaultButton = msgBox.addButton(tr("Use Updated Default"), QMessageBox::ActionRole);
    QObject::connect(useUpdatedDefaultButton, &QPushButton::clicked, &msgBox, [&choice, &msgBox] {
        choice = ExcludesChoice::UseUpdatedDefault;
        msgBox.accept();
    });

    msgBox.exec();
    return choice;
}

void MainWindow::showUpdatedExcludesDialog(const QString &diffOutput) const
{
    QDialog dialog(const_cast<MainWindow *>(this));
    dialog.setWindowTitle(tr("Updated Exclusion List"));

    auto *layout = new QVBoxLayout(&dialog);
    auto *textEdit = new QPlainTextEdit(&dialog);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(diffOutput);
    QFont font(QStringLiteral("monospace"));
    font.setStyleHint(QFont::Monospace);
    textEdit->setFont(font);
    layout->addWidget(textEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, &dialog);
    layout->addWidget(buttons);

    // Highlight diff lines similar to standard diff coloring
    QTextCharFormat addedFormat;
    addedFormat.setForeground(Qt::darkGreen);
    addedFormat.setFontWeight(QFont::Bold);

    QTextCharFormat locationFormat;
    locationFormat.setForeground(QColor(35, 140, 216));
    locationFormat.setFontWeight(QFont::Bold);

    QTextCharFormat removedFormat;
    removedFormat.setForeground(QColor(187, 15, 30));
    removedFormat.setFontWeight(QFont::Bold);

    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor(102, 102, 102));
    headerFormat.setFontWeight(QFont::Bold);

    QTextCursor cursor(textEdit->document());
    cursor.setPosition(0);
    while (!cursor.atEnd()) {
        const QString lineText = cursor.block().text();
        if (lineText.startsWith("@@")) {
            cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(locationFormat);
        } else if (lineText.startsWith("+++ ") || lineText.startsWith("--- ")) {
            cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(headerFormat);
        } else if (lineText.startsWith('+')) {
            cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(addedFormat);
        } else if (lineText.startsWith('-')) {
            cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(removedFormat);
        }
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
    }
    cursor.setPosition(0);

    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.resize(900, 700);
    dialog.exec();
}

bool MainWindow::resetCustomExcludes()
{
    const QString configuredPath = settings->snapshotExcludes.fileName();
    const QString sourcePath = settings->getExcludesSourcePath();

    if (!ExcludesUtils::resetCustomExcludes(configuredPath, sourcePath)) {
        if (!sourcePath.isEmpty() && !configuredPath.isEmpty()) {
            QMessageBox::warning(this, tr("Error"),
                                 tr("Could not replace the exclusion file with the updated default."));
        }
        return false;
    }

    watchExcludesFile();
    return true;
}

void MainWindow::setOtherOptions()
{
    ui->cbCompression->setCurrentIndex(ui->cbCompression->findText(settings->compression, Qt::MatchStartsWith));
    ui->checkMd5->setChecked(settings->makeMd5sum);
    ui->checkSha256->setChecked(settings->makeSha256sum);
    ui->radioRespin->setChecked(settings->resetAccounts);
    ui->spinCPU->setMaximum(static_cast<int>(settings->maxCores));
    ui->spinCPU->setValue(static_cast<int>(settings->cores));
    ui->spinThrottle->setValue(static_cast<int>(settings->throttle));
}

void MainWindow::setConnections()
{
    connect(&timer, &QTimer::timeout, this, &MainWindow::progress);
    connect(&work, &Work::message, this, &MainWindow::processMsg);
    connect(&work, &Work::messageBox, this, &MainWindow::processMsgBox);
    connect(&work.shell, &Cmd::done, this, &MainWindow::procDone);
    connect(&work.shell, &Cmd::errorAvailable, this, [](const QString &out) { qWarning().noquote() << out; });
    connect(&work.shell, &Cmd::outputAvailable, this, [](const QString &out) { qDebug().noquote() << out; });
    connect(&work.shell, &Cmd::started, this, &MainWindow::procStart);
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        // Skip cleanup on aboutToQuit when there's nothing to tear down — the
        // user closed the window before starting work, or cleanup already ran
        // (e.g. the shutdown-when-done path, which calls it eagerly). Checking
        // isDone() here instead of isCleaningUp() would skip teardown (bind-root
        // overlay unmount, log copy, chown_conf) for every successful snapshot
        // that doesn't request a shutdown.
        if (!work.isStarted() || work.isCleaningUp()) {
            return;
        }
        cleanUp();
    });
    connect(ui->btnAbout, &QPushButton::clicked, this, &MainWindow::btnAbout_clicked);
    connect(ui->btnBack, &QPushButton::clicked, this, &MainWindow::btnBack_clicked);
    connect(ui->btnCancel, &QPushButton::clicked, this, &MainWindow::btnCancel_clicked);
    connect(ui->btnEditExclude, &QPushButton::clicked, this, &MainWindow::btnEditExclude_clicked);
    connect(ui->btnRemoveCustomExclude, &QPushButton::clicked, this, &MainWindow::btnRemoveCustomExclude_clicked);
    connect(ui->btnHelp, &QPushButton::clicked, this, &MainWindow::btnHelp_clicked);
    connect(ui->btnNext, &QPushButton::clicked, this, &MainWindow::btnNext_clicked);
    connect(ui->btnSelectSnapshot, &QPushButton::clicked, this, &MainWindow::btnSelectSnapshot_clicked);
    connect(ui->cbCompression, &QComboBox::currentIndexChanged, this,
            &MainWindow::cbCompression_currentIndexChanged);
    connect(ui->checkMd5, &QCheckBox::toggled, this, &MainWindow::checkMd5_toggled);
    connect(ui->checkShutdownOutput, &QCheckBox::toggled, this, [this](bool checked) { settings->shutdown = checked; });
    connect(ui->checkSha256, &QCheckBox::toggled, this, &MainWindow::checkSha256_toggled);
    connect(ui->excludeAll, &QCheckBox::clicked, ui->excludeDesktop, &QCheckBox::setChecked);
    connect(ui->excludeAll, &QCheckBox::clicked, ui->excludeDocuments, &QCheckBox::setChecked);
    connect(ui->excludeAll, &QCheckBox::clicked, ui->excludeDownloads, &QCheckBox::setChecked);
    connect(ui->excludeAll, &QCheckBox::clicked, ui->excludeFlatpaks, &QCheckBox::setChecked);
    connect(ui->excludeAll, &QCheckBox::clicked, ui->excludeMusic, &QCheckBox::setChecked);
    connect(ui->excludeAll, &QCheckBox::clicked, ui->excludeNetworks, &QCheckBox::setChecked);
    connect(ui->excludeAll, &QCheckBox::clicked, ui->excludePictures, &QCheckBox::setChecked);
    connect(ui->excludeAll, &QCheckBox::clicked, ui->excludeSteam, &QCheckBox::setChecked);
    connect(ui->excludeAll, &QCheckBox::clicked, ui->excludeVideos, &QCheckBox::setChecked);
    connect(ui->excludeAll, &QCheckBox::clicked, ui->excludeVirtualBox, &QCheckBox::setChecked);
    for (QCheckBox *cb : {ui->excludeDesktop, ui->excludeDocuments, ui->excludeDownloads,
                           ui->excludeFlatpaks, ui->excludeMusic, ui->excludeNetworks,
                           ui->excludePictures, ui->excludeSteam, ui->excludeVideos,
                           ui->excludeVirtualBox}) {
        connect(cb, &QCheckBox::toggled, this, [this](bool checked) {
            if (!checked) {
                ui->excludeAll->setChecked(false);
            }
        });
    }
    connect(&excludesWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        Q_UNUSED(path);
        updateCustomExcludesButton();
        checkUpdatedDefaultExcludes();
        watchExcludesFile(); // re-add in case the file was recreated
    });
    connect(ui->pushReleaseDate, &QPushButton::clicked, this, [this] {
        QCalendarWidget *calendarWidget = new QCalendarWidget(this);
        calendarWidget->setWindowTitle(tr("Select Release Date"));
        calendarWidget->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint);
        connect(calendarWidget, &QCalendarWidget::activated, this, [this, calendarWidget](const QDate date) {
            ui->pushReleaseDate->setText(date.toString("MMMM dd, yyyy"));
            calendarWidget->deleteLater();
        });
        calendarWidget->show();
    });
    connect(ui->radioPersonal, &QRadioButton::clicked, this, &MainWindow::radioPersonal_clicked);
    connect(ui->radioRespin, &QRadioButton::toggled, this, &MainWindow::radioRespin_toggled);
    connect(ui->spinCPU, &QSpinBox::valueChanged, this, &MainWindow::spinCPU_valueChanged);
    connect(ui->spinThrottle, &QSpinBox::valueChanged, this,
            &MainWindow::spinThrottle_valueChanged);
}

void MainWindow::setExclusions()
{
    QVector<QPair<QCheckBox *, Settings::Exclude>> exclusionPairs
        = {{ui->excludeDesktop, Settings::Exclude::Desktop},     {ui->excludeDocuments, Settings::Exclude::Documents},
           {ui->excludeDownloads, Settings::Exclude::Downloads}, {ui->excludeFlatpaks, Settings::Exclude::Flatpaks},
           {ui->excludeMusic, Settings::Exclude::Music},         {ui->excludeNetworks, Settings::Exclude::Networks},
           {ui->excludePictures, Settings::Exclude::Pictures},   {ui->excludeSteam, Settings::Exclude::Steam},
           {ui->excludeVideos, Settings::Exclude::Videos},       {ui->excludeVirtualBox, Settings::Exclude::VirtualBox}};

    for (const auto &pair : exclusionPairs) {
        pair.first->setChecked(settings->exclusions.testFlag(pair.second));
    }
}

void MainWindow::setup()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    setWindowFlags(Qt::Window); // for the close, min and max buttons
    QFont font("monospace");
    font.setStyleHint(QFont::Monospace);
    ui->outputBox->setFont(font);
    ui->outputBox->setReadOnly(true);

    setWindowTitle(tr("MX Snapshot"));
    ui->btnBack->setHidden(true);
    ui->stackedWidget->setCurrentIndex(0);
    ui->btnCancel->setEnabled(true);
    ui->btnNext->setEnabled(true);
    ui->cbCompression->blockSignals(true);
    ui->cbCompression->addItems({"lz4 - " + tr("fastest, worst compression"), "lzo - " + tr("fast, worse compression"),
                                 "gzip - " + tr("slow, better compression"), "zstd - " + tr("best compromise"),
                                 "xz - " + tr("slowest, best compression")});
    ui->cbCompression->blockSignals(false);
    if (Settings::getDebianVerNum() < Release::Bookworm) {
        ui->labelThrottle->hide();
        ui->spinThrottle->hide();
    }

    show();
}

void MainWindow::listUsedSpace()
{
    show();
    ui->btnNext->setDisabled(true);
    ui->btnCancel->setDisabled(true);
    ui->btnSelectSnapshot->setDisabled(true);
    ui->labelUsedSpace->setText(settings->getUsedSpace());
    ui->btnNext->setEnabled(true);
    ui->btnCancel->setEnabled(true);
    ui->btnSelectSnapshot->setEnabled(true);
}

void MainWindow::listFreeSpace()
{
    qDebug() << "+++" << __PRETTY_FUNCTION__ << "+++";
    const QString path = FileSystemUtils::nearestExistingPath(settings->snapshotDir);
    const QString freeSpace = settings->getFreeSpaceStrings(path);
    ui->labelFreeSpace->clear();
    ui->labelFreeSpace->setText("- " + tr("Free space on %1, where snapshot folder is placed: ").arg(path) + freeSpace
                                + "\n");
    ui->labelDiskSpaceHelp->setText(
        tr("The free space should be sufficient to hold the compressed data from / and /home\n\n"
           "      If necessary, you can create more available space by removing previous snapshots and saved copies: "
           "%1 snapshots are taking up %2 of disk space.")
            //        tr("The free space should be sufficient to hold the compressed data from / and /home\n\n"
            //           "      If necessary, you can create more available space\n"
            //           "      by removing previous snapshots and saved copies:\n"
            //           "      %1 snapshots are taking up %2 of disk space.\n")
            .arg(QString::number(settings->getSnapshotCount()), settings->getSnapshotSize()));
}

bool MainWindow::installPackage(const QString &package)
{
    setWindowTitle(tr("Installing ") + package);
    ui->outputLabel->setText(tr("Installing ") + package);
    ui->outputBox->clear();
    ui->btnNext->setDisabled(true);
    ui->btnBack->setDisabled(true);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    displayOutput();
    if (!work.installPackage(package)) {
        disableOutput();
        return false;
    }
    disableOutput();
    return true;
}

void MainWindow::cleanUp()
{
    // isCleaningUp(), not isDone(): a successful, non-shutdown snapshot is
    // "done" but has not torn down the bind-root overlay yet — that still
    // needs to happen once, whenever the app actually closes.
    if (!work.isStarted() || work.isCleaningUp()) {
        return;
    }
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    work.cleanUp();
}

void MainWindow::cancelFromSignal()
{
    // cleanUp() exits the application when the teardown finishes and is a
    // no-op while one is already running.
    if (work.isStarted()) {
        cleanUp();
        return;
    }
    // Not started: quit. handleSettingsPage() may be inside its elevated
    // checks; when their nested waits unwind it must not start the run.
    quitRequested = true;
    QCoreApplication::quit();
}

// Once the snapshot pipeline is underway a denied re-authentication (the polkit
// grant is only kept for a few minutes) can leave root steps silently skipped,
// so abort through the regular cleanup path instead of continuing with a
// potentially corrupt ISO.
void MainWindow::abortIfElevationDenied()
{
    if (!Cmd::elevationDenied()) {
        return;
    }
    processMsgBox(BoxType::critical, tr("Error"),
                  tr("Administrator access was not granted; the snapshot cannot continue."));
    cleanUp();
}

void MainWindow::procStart()
{
    realProgressActive = false;
    ui->progressBar->setTextVisible(false);
    timer.start(500ms);
    setCursor(QCursor(Qt::BusyCursor));
}

void MainWindow::processMsgBox(BoxType box_type, const QString &title, const QString &msg)
{
    qDebug().noquote() << title << msg;

    QMessageBox msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setText(msg);

    static const QMap<BoxType, QMessageBox::Icon> iconMap = {{BoxType::warning, QMessageBox::Warning},
                                                             {BoxType::critical, QMessageBox::Critical},
                                                             {BoxType::question, QMessageBox::Question},
                                                             {BoxType::information, QMessageBox::Information}};

    msgBox.setIcon(iconMap.value(box_type, QMessageBox::NoIcon));
    msgBox.exec();
}
void MainWindow::processMsg(const QString &msg)
{
    qDebug().noquote() << msg;
    ui->outputLabel->setText(msg);
    ui->checkShutdownOutput->setVisible(msg != tr("Done"));
}

void MainWindow::procDone()
{
    if (!pendingOutputBuffer.isEmpty()) {
        handleOutputLine(pendingOutputBuffer, false);
        pendingOutputBuffer.clear();
    }
    timer.stop();
    ui->progressBar->setValue(ui->progressBar->maximum());
    setCursor(QCursor(Qt::ArrowCursor));
}

void MainWindow::displayOutput()
{
    connect(&work.shell, &Cmd::outputAvailable, this, &MainWindow::outputAvailable);
    connect(&work.shell, &Cmd::errorAvailable, this, &MainWindow::outputAvailable);
}

void MainWindow::disableOutput()
{
    // Disconnect only the slot displayOutput() added. Passing nullptr for the
    // slot would remove every connection from these signals to this object,
    // including the persistent logging lambdas set up in setConnections().
    disconnect(&work.shell, &Cmd::outputAvailable, this, &MainWindow::outputAvailable);
    disconnect(&work.shell, &Cmd::errorAvailable, this, &MainWindow::outputAvailable);
}

void MainWindow::outputAvailable(const QString &out)
{
    bool sawCarriageReturn = false;
    for (const QChar ch : out) {
        if (ch == '\r') {
            sawCarriageReturn = true;
            if (!pendingOutputBuffer.isEmpty()) {
                handleOutputLine(pendingOutputBuffer, true);
                pendingOutputBuffer.clear();
            }
            continue;
        }
        if (ch == '\n') {
            handleOutputLine(pendingOutputBuffer, false);
            pendingOutputBuffer.clear();
            continue;
        }
        pendingOutputBuffer += ch;
    }
    if (sawCarriageReturn && !pendingOutputBuffer.isEmpty()) {
        if (!updateSquashfsProgress(pendingOutputBuffer)) {
            showTransientOutputLine(pendingOutputBuffer);
        }
    }
}

void MainWindow::progress()
{
    if (realProgressActive) {
        return;
    }
    ui->progressBar->setValue((ui->progressBar->value() + 1) % ui->progressBar->maximum() + 1);

    // In live environment and first page, blink text while calculating used disk space
    if (settings->live && (ui->stackedWidget->currentIndex() == 0)) {
        if (ui->progressBar->value() % 4 == 0) {
            ui->labelUsedSpace->setText("\n " + tr("Please wait."));
        } else {
            ui->labelUsedSpace->setText("\n " + tr("Please wait. Calculating used disk space..."));
        }
    }
}

void MainWindow::btnNext_clicked()
{
    QString file_name = ui->lineEditName->text();
    appendIsoExtension(file_name);

    const bool onSetupPage = ui->stackedWidget->currentWidget() == ui->selectionPage
                             || ui->stackedWidget->currentWidget() == ui->settingsPage;
    if (onSetupPage) {
        // Same rules as the CLI: a bad name (e.g. "dir/name") would only fail in
        // xorriso, after the whole squashfs pass.
        if (const QString nameError = Settings::snapshotNameError(file_name); !nameError.isEmpty()) {
            QMessageBox::critical(this, tr("Error"), nameError);
            return;
        }
    }

    if (QFile::exists(settings->snapshotDir + "/" + file_name)) {
        showErrorMessageBox(settings->snapshotDir + "/" + file_name);
        return;
    }

    if (ui->stackedWidget->currentWidget() == ui->selectionPage) {
        handleSelectionPage(file_name);
    } else if (ui->stackedWidget->currentWidget() == ui->settingsPage) {
        handleSettingsPage(file_name);
    } else {
        QApplication::quit();
    }
}

void MainWindow::appendIsoExtension(QString &file_name) const
{
    if (!file_name.endsWith(".iso")) {
        file_name += ".iso";
    }
}

void MainWindow::showErrorMessageBox(const QString &file_path)
{
    QMessageBox::critical(
        this, tr("Error"),
        tr("Output file %1 already exists. Please use another file name, or delete the existent file.").arg(file_path));
}

void MainWindow::handleSelectionPage(const QString &file_name)
{
    if (!settings->validateSpaceRequirements()) {
        processMsgBox(BoxType::critical, tr("Error"),
                      tr("Insufficient free space. Please select a different snapshot directory or free up space."));
        return;
    }

    setWindowTitle(tr("Settings"));
    ui->stackedWidget->setCurrentWidget(ui->settingsPage);
    ui->btnBack->setHidden(false);
    ui->btnBack->setEnabled(true);
    settings->kernel = ui->comboLiveKernel->currentText();
    settings->selectKernel();
    ui->labelTitleSummary->setText(tr("Snapshot will use the following settings:"));

    ui->labelSummary->setText("\n" + tr("- Snapshot directory:") + " " + settings->snapshotDir + "\n" + "- "
                              + tr("Snapshot name:") + " " + file_name + "\n" + tr("- Kernel to be used:") + " "
                              + settings->kernel + "\n");
    settings->snapshotName = file_name; // e.g. Work::cleanUp() names the saved log after it
    settings->codename = ui->textCodename->text();
    settings->distroVersion = ui->textDistroVersion->text();
    settings->projectName = ui->textProjectName->text();
    settings->fullDistroName = settings->projectName + "-" + settings->distroVersion + "_" + QString(settings->x86 ? "386" : "x64");
    settings->bootOptions = ui->textOptions->text();
    settings->releaseDate = ui->pushReleaseDate->text();
    checkNvidiaGraphicsCard();
    settings->bootOptions = ui->textOptions->text();
    checkUpdatedDefaultExcludes();
}

void MainWindow::checkNvidiaGraphicsCard()
{
    static bool hasRun = false;
    QString currentOptions = ui->textOptions->text();

    if (hasRun || currentOptions.contains("xorg=nvidia")) {
        return;
    }

    if (ExcludesUtils::hasNvidiaGraphicsCard(work.shell)) {
        if (QMessageBox::Yes
            == QMessageBox::question(this, tr("NVIDIA Detected"),
                                     tr("This computer uses an NVIDIA graphics card. Are you planning to use the "
                                        "resulting ISO on the same computer or another computer with an NVIDIA card?"),
                                     QMessageBox::Yes | QMessageBox::No)) {
            ui->textOptions->setText(currentOptions.isEmpty() ? "xorg=nvidia" : currentOptions + " xorg=nvidia");
            QMessageBox::information(this, tr("NVIDIA Selected"),
                                     tr("Note: If you use the resulting ISO on a computer without an NVIDIA card, "
                                        "you will likely need to remove 'xorg=nvidia' from the boot options."));
        } else {
            QMessageBox::information(this, tr("NVIDIA Detected"),
                                     tr("Note: If you use the resulting ISO on a computer with an NVIDIA card, "
                                        "you may need to add 'xorg=nvidia' to the boot options."));
        }
    }
    hasRun = true;
}

void MainWindow::checkUpdatedDefaultExcludes()
{
    QString diffOutput;
    if (!isSourceExcludesNewer(diffOutput)) {
        return;
    }

    const QString configuredPath = settings->snapshotExcludes.fileName();
    const QString sourcePath = settings->getExcludesSourcePath();
    ExcludesChoice choice = ExcludesChoice::None;

    while (true) {
        choice = showUpdatedExcludesPrompt(configuredPath, sourcePath);
        if (choice == ExcludesChoice::ShowDiff) {
            showUpdatedExcludesDialog(diffOutput);
            continue; // re-prompt after showing diff
        }
        if (choice == ExcludesChoice::None) {
            return; // user closed the prompt; do nothing
        }
        break;
    }

    if (choice == ExcludesChoice::UseUpdatedDefault) {
        if (resetCustomExcludes()) {
            updateCustomExcludesButton();
        } else {
            QMessageBox::warning(this, tr("Error"),
                                 tr("Could not replace the exclusion file with the updated default."));
        }
    } else if (choice == ExcludesChoice::KeepCustom) {
        [[maybe_unused]] const bool touched = ExcludesUtils::touchExcludesTimestamp(configuredPath);
    }
}

void MainWindow::handleSettingsPage(const QString &file_name)
{
    if (!settings->checkCompression()) {
        processMsgBox(BoxType::critical, tr("Error"),
                      tr("Current kernel doesn't support selected compression algorithm, please edit the "
                         "configuration file and select a different algorithm."));
        return;
    }

    if (!confirmUnitCredentials() || !confirmStart()) {
        return;
    }

    // These are the first operations that ask for administrator access. If the
    // user cancels the authentication dialog (or it fails otherwise), stay on
    // the settings page so they can simply press Next again — don't kill the app.
    // While authentication is pending the nested event loop still delivers
    // clicks: disable the buttons so Next cannot start a second run, and
    // closeApp() refuses to quit underneath the wait.
    elevatedSetupInProgress = true;
    ui->btnNext->setEnabled(false);
    ui->btnBack->setEnabled(false);
    ui->btnCancel->setEnabled(false);
    Cmd::clearElevationDenied();
    const bool dirsOk = settings->checkSnapshotDir() && settings->checkTempDir();
    if (dirsOk) {
        applyExclusions();
    }
    elevatedSetupInProgress = false;
    if (quitRequested) {
        settings->tmpdir.reset();
        return;
    }
    ui->btnCancel->setEnabled(true);
    if (Cmd::elevationDenied() || !dirsOk) {
        ui->btnNext->setEnabled(true);
        ui->btnBack->setEnabled(true);
        settings->tmpdir.reset(); // Drop any created work dir; a retry recreates it
        if (Cmd::elevationDenied()) {
            processMsgBox(BoxType::warning, tr("Cancelled"),
                          tr("Administrator access is required to create a snapshot.") + '\n'
                              + tr("The operation was cancelled. Select Next when you are ready to try again."));
        } else {
            processMsgBox(BoxType::critical, tr("Error"),
                          tr("Could not create the snapshot or work directory."));
        }
        return;
    }

    work.startTimer();
    prepareForOutput(file_name);
}

// A respin is meant for other computers, but services the admin set up in
// /etc/systemd/system go into it as they are, including any tokens or keys in
// their Environment= lines. Name them (and any files this unelevated scan
// couldn't read, which are copied all the same) and let the user stop.
bool MainWindow::confirmUnitCredentials()
{
    if (!settings->resetAccounts) {
        return true;
    }
    const UnitSecrets::ScanResult scan = UnitSecrets::findCredentials(QStringLiteral("/etc/systemd/system"));
    if (scan.findings.isEmpty() && scan.unreadable.isEmpty()) {
        return true;
    }
    QString text;
    if (!scan.findings.isEmpty()) {
        qWarning().noquote() << "Services with credentials will be included in the snapshot:"
                             << scan.findings.join(", ");
        text += tr("These services in /etc/systemd/system contain what look like passwords, tokens or keys, and "
                   "will be included in the snapshot as they are:")
                + "\n\n" + scan.findings.join('\n') + "\n\n";
    }
    if (!scan.unreadable.isEmpty()) {
        qWarning().noquote() << "Could not check these service files for credentials:" << scan.unreadable.join(", ");
        text += tr("These service files or folders could not be checked for passwords, tokens or keys, and will be included "
                   "in the snapshot as they are:")
                + "\n\n" + scan.unreadable.join('\n') + "\n\n";
    }
    text += tr("Every system started or installed from this ISO will have them. If the ISO will be shared, remove "
               "the credentials from these files or add the files to the exclusion list. Disabling a service is "
               "not enough: its files are still copied.")
            + "\n\n" + tr("Continue anyway?");
    return QMessageBox::Yes
           == QMessageBox::warning(this, tr("Credentials in services"), text, QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);
}

bool MainWindow::confirmStart()
{
    QMessageBox messageBox(this);
    messageBox.setIcon(QMessageBox::Question);
    messageBox.setWindowTitle(tr("Final chance"));
    messageBox.setText(
        tr("Snapshot now has all the information it needs to create an ISO from your running system.") + "\n\n"
        + tr("It will take some time to finish, depending on the size of the installed system and the capacity of "
             "your computer.")
        + "\n\n" + tr("OK to start?"));
    auto *pushOk = messageBox.addButton(QMessageBox::Ok);
    messageBox.addButton(QMessageBox::Cancel);
    const QString mountWarning = SystemInfo::systemMountWarning();
    if (!mountWarning.isEmpty()) {
        messageBox.setText(mountWarning + "\n\n" + messageBox.text());
        messageBox.setIcon(QMessageBox::Warning);
        messageBox.setDefaultButton(QMessageBox::Cancel);
    }
    auto *checkShutdown = new QCheckBox(this);
    checkShutdown->setText(tr("Shutdown computer when done."));
    if (settings->shutdown) {
        checkShutdown->setCheckState(Qt::Checked);
    }
    messageBox.setCheckBox(checkShutdown);
    messageBox.exec();
    // Only an explicit OK starts the run: a box closed because the application
    // is quitting returns with no button clicked.
    if (messageBox.clickedButton() != pushOk) {
        return false;
    }
    settings->shutdown = checkShutdown->isChecked();
    ui->checkShutdownOutput->setChecked(settings->shutdown);
    return true;
}

void MainWindow::applyExclusions()
{
    settings->excludeDesktop(ui->excludeDesktop->isChecked());
    settings->excludeDocuments(ui->excludeDocuments->isChecked());
    settings->excludeDownloads(ui->excludeDownloads->isChecked());
    settings->excludeFlatpaks(ui->excludeFlatpaks->isChecked());
    settings->excludeMusic(ui->excludeMusic->isChecked());
    settings->excludeNetworks(ui->excludeNetworks->isChecked());
    settings->excludePictures(ui->excludePictures->isChecked());
    settings->excludeSteam(ui->excludeSteam->isChecked());
    settings->excludeVideos(ui->excludeVideos->isChecked());
    settings->excludeVirtualBox(ui->excludeVirtualBox->isChecked());
    settings->otherExclusions();
}

void MainWindow::prepareForOutput(const QString &file_name)
{
    ui->btnNext->setEnabled(false);
    ui->btnBack->setEnabled(false);
    ui->stackedWidget->setCurrentWidget(ui->outputPage);
    setWindowTitle(tr("Output"));
    ui->checkShutdownOutput->setChecked(settings->shutdown);
    ui->checkShutdownOutput->setVisible(true);
    ui->outputBox->clear();
    pendingOutputBuffer.clear();
    transientOutputLineActive = false;
    // Each stage may trigger cleanUp() (which is no longer [[noreturn]] —
    // see Step 4d). Bail out between stages when that happens, otherwise
    // we run the rest of the pipeline against a torn-down bind-root.
    work.setupEnv();
    abortIfElevationDenied();
    if (work.isCleaningUp()) {
        return;
    }
    if (!settings->monthly && !settings->overrideSize) {
        work.checkEnoughSpace();
        if (work.isCleaningUp()) {
            return;
        }
    }
    // checkEnoughSpace() can relocate the work dir onto another partition, which
    // re-runs setupEnv() (and its own privileged steps) internally — check again
    // here in case that nested run was the one that got denied.
    abortIfElevationDenied();
    if (work.isCleaningUp()) {
        return;
    }
    work.copyNewIso();
    abortIfElevationDenied();
    if (work.isCleaningUp()) {
        return;
    }
    ui->outputLabel->clear();
    work.savePackageList(file_name);
    if (work.isCleaningUp()) {
        return;
    }

    if (settings->editBootMenu) {
        editBootMenu();
    }

    displayOutput();
    TerminalCursor cursor(STDOUT_FILENO);
    work.createIso(file_name);
    ui->btnCancel->setText(tr("Close"));
}

void MainWindow::appendOutputLine(const QString &line)
{
    ui->outputBox->moveCursor(QTextCursor::End);
    ui->outputBox->insertPlainText(line + '\n');
    ui->outputBox->verticalScrollBar()->setValue(ui->outputBox->verticalScrollBar()->maximum());
    transientOutputLineActive = false;
}

void MainWindow::handleOutputLine(const QString &line, bool transientHint)
{
    if (updateSquashfsProgress(line)) {
        return;
    }
    if (line.startsWith("xorriso : UPDATE :") || transientHint) {
        showTransientOutputLine(line);
        return;
    }
    if (line.isEmpty() && transientOutputLineActive) {
        return;
    }
    appendOutputLine(line);
}

bool MainWindow::updateSquashfsProgress(const QString &line)
{
    bool ok = false;
    const int percentage = SquashfsUtils::parsePercentageLine(line, &ok);
    if (!ok) {
        return false;
    }

    realProgressActive = true;
    timer.stop();
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setTextVisible(true);
    ui->progressBar->setFormat(QStringLiteral("%p%"));
    ui->progressBar->setValue(percentage);
    showTransientOutputLine(QStringLiteral(" %1%").arg(percentage));
    return true;
}

void MainWindow::showTransientOutputLine(const QString &line)
{
    QTextDocument *document = ui->outputBox->document();
    if (!transientOutputLineActive) {
        ui->outputBox->moveCursor(QTextCursor::End);
        ui->outputBox->insertPlainText(line + '\n');
        ui->outputBox->verticalScrollBar()->setValue(ui->outputBox->verticalScrollBar()->maximum());
        transientOutputLineActive = true;
        return;
    }

    // The trailing newline keeps an empty last block in the document,
    // so the displayed transient line is the second-to-last block.
    QTextBlock block = document->findBlockByNumber(document->blockCount() - 2);
    if (!block.isValid()) {
        appendOutputLine(line);
        transientOutputLineActive = true;
        return;
    }

    QTextCursor cursor(block);
    cursor.select(QTextCursor::LineUnderCursor);
    cursor.removeSelectedText();
    cursor.insertText(line);
    ui->outputBox->verticalScrollBar()->setValue(ui->outputBox->verticalScrollBar()->maximum());
}

void MainWindow::editBootMenu()
{
    if (QMessageBox::Yes
        == QMessageBox::question(
            this, tr("Edit Boot Menu"),
            tr("The program will now pause to allow you to edit any files in the work directory. "
               "Select Yes to edit the boot menu or select No to bypass this step and continue creating the "
               "snapshot."),
            QMessageBox::Yes | QMessageBox::No)) {
        hide();
        // getEditorCommand() is a resolved argument list executed directly (no
        // shell), so neither the configured editor nor the workDir-derived file
        // paths can inject commands.
        const QString bootDir = settings->workDir + "/iso-template/boot";
        QStringList editorCmd = settings->getEditorCommand();
        const QString editorProgram = editorCmd.takeFirst();
        work.shell.proc(editorProgram, editorCmd << bootDir + "/grub/grub.cfg" << bootDir + "/syslinux/syslinux.cfg"
                                                 << bootDir + "/isolinux/isolinux.cfg");
        show();
    }
}

void MainWindow::btnBack_clicked()
{
    setWindowTitle(tr("MX Snapshot"));
    ui->stackedWidget->setCurrentIndex(0);
    ui->btnNext->setEnabled(true);
    ui->btnBack->setHidden(true);
    ui->outputBox->clear();
}

void MainWindow::btnEditExclude_clicked()
{
    hide();
    Cmd editor(this);
    // getEditorCommand() is a resolved argument list executed directly (no
    // shell), so neither the configured editor nor the user-configurable
    // excludes path can inject commands.
    QStringList editorCmd = settings->getEditorCommand();
    const QString editorProgram = editorCmd.takeFirst();
    editor.proc(editorProgram, editorCmd << settings->snapshotExcludes.fileName());
    updateCustomExcludesButton();
    checkUpdatedDefaultExcludes();
    show();
}

void MainWindow::btnRemoveCustomExclude_clicked()
{
    const QMessageBox::StandardButton response = QMessageBox::question(
        this, tr("Remove Custom Exclusion File"),
        tr("Revert the exclusion list to the default file? This will overwrite your current exclusions."),
        QMessageBox::Yes | QMessageBox::No);
    if (response != QMessageBox::Yes) {
        return;
    }

    if (resetCustomExcludes()) {
        updateCustomExcludesButton();
    }
}


void MainWindow::radioRespin_toggled(bool checked)
{
    settings->resetAccounts = checked;
    if (checked && !ui->excludeAll->isChecked()) {
        ui->excludeAll->click();
    }
}

void MainWindow::radioPersonal_clicked(bool checked)
{
    settings->resetAccounts = !checked;
    if (checked && ui->excludeAll->isChecked()) {
        ui->excludeAll->click();
    }
}

void MainWindow::btnAbout_clicked()
{
    hide();
    displayAboutMsgBox(
        tr("About %1").arg(QApplication::applicationDisplayName()),
        "<p align=\"center\"><b><h2>" + QApplication::applicationDisplayName() + "</h2></b></p><p align=\"center\">"
            + tr("Version: ") + QApplication::applicationVersion() + "</p><p align=\"center\"><h3>"
            + tr("Program for creating a live-CD from the running system for MX Linux")
            + R"(</h3></p><p align="center"><a href="http://mxlinux.org">http://mxlinux.org</a><br /></p><p align="center">)"
            + tr("Copyright (c) MX Linux") + "<br /><br /></p>",
        QStringLiteral("/usr/share/doc/mx-snapshot/license.html"),
        tr("%1 License").arg(QApplication::applicationDisplayName()));
    show();
}

void MainWindow::btnHelp_clicked()
{
    QLocale locale;
    QString path {"/usr/share/doc/mx-snapshot/mx-snapshot.html"};

    if (locale.bcp47Name().startsWith("fr")) {
        path = "/usr/share/doc/mx-snapshot/mx-snapshot-fr.html";
    }

    displayHelpDoc(path, tr("%1 Help").arg(windowTitle()));
}

void MainWindow::btnSelectSnapshot_clicked()
{
    QString selected = QFileDialog::getExistingDirectory(this, tr("Select Snapshot Directory"), QString(),
                                                         QFileDialog::ShowDirsOnly);
    if (!selected.isEmpty()) {
        const QString previousSnapshotDir = settings->snapshotDir;
        settings->snapshotDir = selected + "/snapshot";
        if (!settings->validateSpaceRequirements()) {
            settings->snapshotDir = previousSnapshotDir;
            QMessageBox::critical(this, tr("Error"),
                                  tr("Insufficient free space in the selected directory. Please choose a different location."));
            return;
        }
        ui->labelSnapshotDir->setText(settings->snapshotDir);
        listFreeSpace();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        closeApp();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Programmatic close (our own close() at the end of closeApp, or the
    // application quitting): just let it through.
    if (!event->spontaneous()) {
        event->accept();
        return;
    }
    // Spontaneous close (X button, window manager): route through closeApp so
    // we get the confirmation prompt and the right cleanup behavior.
    if (!closeApp(true)) {
        event->ignore();
        return;
    }
    event->accept();
}

bool MainWindow::closeApp(bool fromCloseEvent)
{
    // Closing the last window quits the event loops that the running teardown
    // or authentication wait depends on. cleanUp() exits the app when it is
    // done; a pending authentication returns control to the settings page.
    if (work.isCleanupRunning() || elevatedSetupInProgress) {
        return false;
    }
    // Ask for confirmation when on outputPage and not done
    if (ui->stackedWidget->currentWidget() == ui->outputPage && !work.isDone()) {
        if (QMessageBox::Yes
            != QMessageBox::question(this, tr("Confirmation"), tr("Are you sure you want to quit the application?"),
                                     QMessageBox::Yes | QMessageBox::No)) {
            return false;
        }
    }
    qt_settings.setValue("geometry", saveGeometry());

    // If a snapshot is still in flight, or finished but its bind-root overlay
    // teardown hasn't run yet (isCleaningUp(), not isDone() -- a successful
    // non-shutdown snapshot is "done" without having torn anything down), kick
    // off cleanup and keep the window open — the cleanup will close us via
    // QCoreApplication::exit when done.
    const bool needsCleanup = work.isStarted() && !work.isCleaningUp();
    if (needsCleanup) {
        cleanUp();
        return false;
    }
    // Nothing left to tear down: close the window. closeEvent ignores
    // programmatic closes so this won't recurse.
    if (!fromCloseEvent) {
        close();
    }
    return true;
}

void MainWindow::btnCancel_clicked()
{
    closeApp();
}

void MainWindow::cbCompression_currentIndexChanged()
{
    QString comp = ui->cbCompression->currentText().section(" ", 0, 0);
    qt_settings.setValue("compression", comp);
    settings->compression = comp;
}


void MainWindow::checkMd5_toggled(bool checked)
{
    qt_settings.setValue("make_md5sum", checked ? "yes" : "no");
    settings->makeMd5sum = checked;
}

void MainWindow::checkSha256_toggled(bool checked)
{
    qt_settings.setValue("make_sha256sum", checked ? "yes" : "no");
    settings->makeSha256sum = checked;
}


void MainWindow::spinCPU_valueChanged(int arg1)
{
    qt_settings.setValue("cores", arg1);
    settings->cores = arg1;
}

void MainWindow::spinThrottle_valueChanged(int arg1)
{
    qt_settings.setValue("throttle", arg1);
    settings->throttle = arg1;
}
