#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "usbdevicelistmodel.h"
#include "upgradeThread.h"



#if TREZOR_TEST == 0
#define KEYBOX2_VENDOR_ID 0xb6ab
#define KEYBOX2_PRODUCT_ID 0xbaeb
#define KEYBOX2_BCD_DEVICE 0x0001
#else  // using trezor for debug
#define KEYBOX2_VENDOR_ID 0x534c
#define KEYBOX2_PRODUCT_ID 0x0001
#define KEYBOX2_BCD_DEVICE 0x0100
#endif

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void upgradeStatusChanged(const UpgradeThread::UpgradeProgress & progress);

private:
    Ui::MainWindow *ui;
    void scanUSBDevices();
    void selectUpgradeFile();
    void upgrade();

    QString mUpgradeFilePath;
    UsbDeviceListModel *mDevList;
    UpgradeThread *mThread;

};

#endif // MAINWINDOW_H
