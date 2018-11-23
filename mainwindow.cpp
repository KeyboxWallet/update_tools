#include <QFileDialog>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <libusb.h>
#include <QDir>
#include <QtDebug>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
	auto appPath = QCoreApplication::applicationDirPath();
#ifdef _WIN32
	QIcon icon(appPath + "/icon.ico");
	setWindowIcon(icon);
#endif
    int ret;

    ret = libusb_init(NULL);
    ui->setupUi(this);
    mUpgradeFilePath = "";
    connect(ui->seletFileButton, &QPushButton::clicked, this, &MainWindow::selectUpgradeFile);
    connect(ui->upgradeButton, &QPushButton::clicked, this, &MainWindow::upgrade);

    ui->upgradeProcess->setMinimum(0);
    ui->upgradeProcess->setMaximum(100);
    ui->upgradeProcess->setValue(0);

    qRegisterMetaType<UpgradeThread::UpgradeProgress>();
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::selectUpgradeFile()
{
    mUpgradeFilePath = QFileDialog::getOpenFileName(this, tr("Select upgrade file"));
    auto parts = mUpgradeFilePath.split(QDir::separator());
    auto fileName = parts[parts.size() - 1];
    ui->fileNameEdit->setText(fileName);
    ui->fileNameEdit->setEnabled(false);
}

void MainWindow::upgrade()
{
    // readFile
    if( mUpgradeFilePath.isEmpty() || mUpgradeFilePath.isNull()){
        ui->upgradeStatus ->setText(tr("Please select upgrade file."));
        return;
    }

    QFile file(mUpgradeFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        ui->upgradeStatus ->setText(tr("can't open file."));
        return;
    }
    QByteArray blob = file.readAll();

    if( blob.length() < 1024){
        ui->upgradeStatus ->setText(tr("file too small."));
        return;
    }

    ui->upgradeButton->setDisabled(true);
    mThread = new UpgradeThread(this,blob);
    connect(mThread, &UpgradeThread::stateChanged, this, &MainWindow::upgradeStatusChanged, Qt::BlockingQueuedConnection);
    connect(mThread, &QThread::finished, mThread, &QObject::deleteLater);

    mThread->start();
}

void MainWindow::upgradeStatusChanged(const UpgradeThread::UpgradeProgress & progress)
{
    qDebug() << "changed:" << progress.state;
    UpgradeThread::UpgradeState state = progress.state;
    if( state == UpgradeThread::UPGRADE_FILE_CONTENT_INVALID) {
        ui->upgradeStatus ->setText(tr("invalid file").append(progress.description));
        ui->upgradeButton->setEnabled(true);
        update();
        return;
    }
    if (state == UpgradeThread::UPGRADE_WAIT_USB_DEVICE) {
        ui->upgradeStatus->setText(tr("scanning keybox, please connect it to this pc via USB"));
        update();
        return;
    }
    if (state == UpgradeThread::UPGRADE_USB_IO_ERROR) {
        ui->upgradeStatus ->setText(tr("Upgrade: IO error:").append(progress.description));
        ui->upgradeButton->setEnabled(true);
        update();
        return;
    }

    if (state == UpgradeThread::UPGRADE_USB_PROTOCOL_ERROR) {
        ui->upgradeStatus->setText(tr("Upgrade Failed: protocol error:").append(progress.description));
        ui->upgradeButton->setEnabled(true);
        update();
        return;
    }
    if (state == UpgradeThread::UPGRADE_DEVICE_STATE_ERROR) {
        ui->upgradeStatus->setText(tr("Please reboot device to bootloader mode:").append(progress.description));
        ui->upgradeButton->setEnabled(true);
        update();
        return;
    }
    if (state == UpgradeThread::UPGRADE_SENDING_REQUEST) {
        ui->upgradeProcess->setValue(2);
        ui->upgradeStatus->setText(tr("Upgrade Request sent, please agree on device."));
        ui->versionInfo->setText(tr("Device version: %1, target version: %2")
                                 .arg(progress.currentFirmwareVersion)
                                 .arg(progress.targetFirmwareVersion));
        update();
        return;
    }

    if (state == UpgradeThread::UPGRADE_USER_REJECTED) {
        ui->upgradeStatus ->setText(tr("Upgrade Failed，Rejected by user:").append(progress.description));
        ui->upgradeButton->setEnabled(true);
        update();
        return;
    }

    if(state == UpgradeThread::UPGRADE_SENDING_CONTENT) {
         ui->upgradeProcess->setValue(10 + (int)80*progress.currentSendOffset / progress.upgradeFileLen);
         ui->upgradeStatus ->setText(tr("Sending firmware..."));
         update();
         return;
    }
    if(state == UpgradeThread::UPGRADE_SENT_FINISHED) {
        ui->upgradeProcess->setValue(90);
        ui->upgradeStatus->setText(tr("Send finished, waiting confirm."));
        update();
        return;
    }
    if(state == UpgradeThread::UPGRADE_WAITING_DEVICE_WRITE) {
        ui->upgradeProcess->setValue(100);
        ui->upgradeStatus->setText(tr("Please watch device and wait for upgrade complete."));
        ui->upgradeButton->setEnabled(true);
        update();
        return;
    }
}
