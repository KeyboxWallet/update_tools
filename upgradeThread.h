#ifndef _UPGRADE_THREAD_INCLUDE_
#define _UPGRADE_THREAD_INCLUDE_

#define TREZOR_TEST 0
#define PROTO_MOCK_TEST 0

#include <QThread>
#include <libusb.h>

#define KEYBOX2_VENDOR_ID 0xb6ab
#define KEYBOX2_PRODUCT_ID 0xbaeb
#define KEYBOX2_BCD_DEVICE 0x0001

class UpgradeThread : public QThread
{
    Q_OBJECT

 public:
    enum UpgradeState{
        UPGRADE_FILE_CONTENT_INVALID,
        UPGRADE_WAIT_USB_DEVICE,
        UPGRADE_USB_DEVICE_TOO_MANY,
        UPGRADE_DEVICE_STATE_ERROR,
        UPGRADE_SENDING_REQUEST, // 第一步
        UPGRADE_SENDING_CONTENT, // 第二步
        UPGRADE_SENT_FINISHED,   // 发送完成
        UPGRADE_USER_REJECTED,
        UPGRADE_WAITING_DEVICE_WRITE, // 第三步
        UPGRADE_USB_PROTOCOL_ERROR,
        UPGRADE_USB_IO_ERROR,
        UPGRADE_FINISHED, // 第四步
    };
    struct UpgradeProgress {
       UpgradeState state;
       QString description;
       QString targetFirmwareVersion;
       QString currentFirmwareVersion;
       int upgradeFileLen;
       int currentSendOffset;
    };
public:
    UpgradeThread(QObject *parent, 
        const QByteArray &upgradeFileContent
    );
    ~UpgradeThread();
    void cancel();

protected:
    void run() override;
signals:
    void stateChanged(const UpgradeThread::UpgradeProgress &s);

private:
    bool mCancelled;
    QByteArray mFileContent;

    // step 1:
    bool waitDeviceConnect();

    // step 2
    bool sendLockSerialCmd();
    // step 3
    bool readDeviceReply();
    // final stage
    void cleanup();

    // return 0, no device
    // return 1, one device
    // return 2, two device
    int scanUsbDevice();

    bool checkDeviceMode();
    // run state
    libusb_device * mDevice;
    libusb_device_handle *mHandle;
    UpgradeProgress mProgress;
};

Q_DECLARE_METATYPE(UpgradeThread::UpgradeProgress);




#endif
