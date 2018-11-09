#include <QFileDialog>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <libusb.h>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    int ret;

    ret = libusb_init(NULL);
    ui->setupUi(this);
    mUpgradeFilePath = QString::fromUtf8("");
    /// ui->listView->setModel(mDevList);
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
    mUpgradeFilePath = QFileDialog::getOpenFileName(this, "选择升级文件");
    ui->fileNameEdit->setText(mUpgradeFilePath);
}

void MainWindow::upgrade()
{
    // readFile
    if( mUpgradeFilePath.isEmpty() || mUpgradeFilePath.isNull()){
        ui->upgradeStatus ->setText(QString::fromUtf8("请选择升级文件"));
        return;
    }

    QFile file(mUpgradeFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        ui->upgradeStatus ->setText(QString::fromUtf8("无法打开文件"));
        return;
    }
    QByteArray blob = file.readAll();

    if( blob.length() < 1024){
        ui->upgradeStatus ->setText(QString::fromUtf8("文件太小"));
        return;
    }

    ui->upgradeButton->setDisabled(true);
    mThread = new UpgradeThread(this,blob);
    connect(mThread, &UpgradeThread::stateChanged, this, &MainWindow::upgradeStatusChanged);
    connect(mThread, &QThread::finished, mThread, &QObject::deleteLater);

    mThread->start();
}

void MainWindow::upgradeStatusChanged(const UpgradeThread::UpgradeProgress & progress)
{
    UpgradeThread::UpgradeState state = progress.state;
    if( state == UpgradeThread::UPGRADE_FILE_CONTENT_INVALID) {
        ui->upgradeStatus ->setText(QString::fromUtf8("非法文件").append(progress.description));
        ui->upgradeButton->setEnabled(true);
        return;
    }
    if (state == UpgradeThread::UPGRADE_WAIT_USB_DEVICE) {
        ui->upgradeStatus->setText(QString::fromUtf8("扫描keybox中，请确保接入设备"));
        return;
    }
    if (state == UpgradeThread::UPGRADE_USB_IO_ERROR) {
        ui->upgradeStatus ->setText(QString::fromUtf8("读取失败:IO错误:").append(progress.description));
        ui->upgradeButton->setEnabled(true);
        return;
    }

    if (state == UpgradeThread::UPGRADE_USB_IO_ERROR) {
        ui->upgradeStatus ->setText(QString::fromUtf8("升级失败:IO错误:").append(progress.description));
        ui->upgradeButton->setEnabled(true);
        return;
    }
    if (state == UpgradeThread::UPGRADE_USB_PROTOCOL_ERROR) {
        ui->upgradeStatus->setText(QString::fromUtf8("升级失败，协议错误").append(progress.description));
        ui->upgradeButton->setEnabled(true);
        return;
    }
    if (state == UpgradeThread::UPGRADE_DEVICE_STATE_ERROR) {
        ui->upgradeStatus->setText(QString::fromUtf8("请操作设备，重启到bootloader模式").append(progress.description));
        ui->upgradeButton->setEnabled(true);
        return;
    }
    if (state == UpgradeThread::UPGRADE_SENDING_REQUEST) {
        ui->upgradeProcess->setValue(2);
        ui->upgradeStatus->setText(QString::fromUtf8("已请求，请同意升级"));
        ui->versionInfo->setText(QString::fromUtf8("设备版本: %1, 目标版本: %2")
                                 .arg(progress.currentFirmwareVersion)
                                 .arg(progress.targetFirmwareVersion));
        return;
    }

    if (state == UpgradeThread::UPGRADE_USER_REJECTED) {
        ui->upgradeStatus ->setText(QString::fromUtf8("升级失败，已被拒绝:").append(progress.description));
        ui->upgradeButton->setEnabled(true);
        return;
    }

    if(state == UpgradeThread::UPGRADE_SENDING_CONTENT) {
         ui->upgradeProcess->setValue(10 + (int)80*progress.currentSendOffset / progress.upgradeFileLen);
         ui->upgradeStatus ->setText(QString::fromUtf8("正在发送升级文件..."));
         return;
    }
    if(state == UpgradeThread::UPGRADE_SENT_FINISHED) {
        ui->upgradeProcess->setValue(90);
        ui->upgradeStatus->setText(QString::fromUtf8("发送完成，等待设备确认..."));
        return;
    }
    if(state == UpgradeThread::UPGRADE_WAITING_DEVICE_WRITE) {
        ui->upgradeProcess->setValue(100);
        ui->upgradeStatus->setText(QString::fromUtf8("请观察设备，烧写完新固件则升级成功"));
        ui->upgradeButton->setEnabled(true);
        return;
    }

    /*
    if(state == UpgradeThread::UPGRADE_FINISHED){
        ui->upgradeProcess->setValue(100);
        ui->upgradeStatus->setText(QString::fromUtf8("升级成功"));
        ui->upgradeButton->setEnabled(true);
        return;
    }*/
}
