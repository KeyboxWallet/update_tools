#ifndef _UPGRADE_THREAD_INCLUDE_
#define _UPGRADE_THREAD_INCLUDE_

#define TREZOR_TEST 0
#define PROTO_MOCK_TEST 0

#include <QThread>
#include <libusb.h>

class UpgradeThread : public QThread
{
    Q_OBJECT

 public:
    enum UpgradeState{
        UPGRADE_FILE_CONTENT_INVALID,
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
        const QByteArray &upgradeFileContent,
        libusb_device * dev
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
    libusb_device * mDevice;
};

Q_DECLARE_METATYPE(UpgradeThread::UpgradeProgress);




#endif
