#include <QFileDialog>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <libusb.h>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    int ret;

    mDevList = new UsbDeviceListModel();
    ret = libusb_init(NULL);
    ui->setupUi(this);
    mUpgradeFilePath = QString::fromUtf8("");
    /// ui->listView->setModel(mDevList);
    connect(ui->seletFileButton, &QPushButton::clicked, this, &MainWindow::selectUpgradeFile);
    connect(ui->reScanButton, &QPushButton::clicked, this, &MainWindow::scanUSBDevices);
    connect(ui->upgradeButton, &QPushButton::clicked, this, &MainWindow::upgrade);

    ui->upgradeProcess->setMinimum(0);
    ui->upgradeProcess->setMaximum(100);
    ui->upgradeProcess->setValue(0);
    ui->listView->setSelectionMode(QAbstractItemView::SingleSelection);

    qRegisterMetaType<UpgradeThread::UpgradeProgress>();
}

MainWindow::~MainWindow()
{
    delete ui;
    delete mDevList;
}

void MainWindow::scanUSBDevices()
{
    libusb_device * keybox_devices[5];
    libusb_device **list;
    // libusb_device *found = NULL;
     ssize_t cnt = libusb_get_device_list(NULL, &list);

    libusb_device * device;
     struct libusb_device_descriptor descriptor;

     int dev_cnt = 0;
     for(int i=0; i<cnt && dev_cnt < 5; i++){
         device = list[i];
         int err = libusb_get_device_descriptor(device, &descriptor);

         if (err) {
             continue;
         }
         if (    descriptor.idVendor == KEYBOX2_VENDOR_ID
             &&  descriptor.idProduct == KEYBOX2_PRODUCT_ID
             &&  descriptor.bcdDevice == KEYBOX2_BCD_DEVICE
             ) {
                 keybox_devices[dev_cnt++] = libusb_ref_device(device);
         }
     }
    /*
    UsbDeviceListModel *model = new QStringListModel();
    QStringList l;
    */
    ui->usbCountLabel->setText(QString::fromUtf8("usb:%1").arg( dev_cnt));
     emit mDevList->layoutAboutToBeChanged();
     mDevList->setAllData(keybox_devices, dev_cnt);
     emit mDevList->layoutChanged();
    libusb_free_device_list(list, true);
     ui->listView->setModel(mDevList);
}


void MainWindow::selectUpgradeFile()
{
    mUpgradeFilePath = QFileDialog::getOpenFileName(this, "选择升级文件");
    ui->fileNameEdit->setText(mUpgradeFilePath);
}

void MainWindow::upgrade()
{
    QItemSelectionModel * model = ui->listView->selectionModel();
    QModelIndexList indexes;
    if( model )
        indexes = model ->selectedIndexes();
    if(indexes.size() != 1){
        ui->upgradeStatus->setText(QString::fromUtf8("请选择一个设备"));
        return;
    }

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
    mThread = new UpgradeThread(this,blob, mDevList->getDevice( indexes[0].row()) );
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
