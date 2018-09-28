#include "upgradeThread.h"
#include "keybox-proto-types.h"
#include "keybox-errcodes.h"
#include "messages.pb.h"
#include <QtDebug>

struct membuf : std::streambuf
{
    membuf(char *begin, char *end)
    {
        this->setg(begin, begin, end);
    }
};


UpgradeThread::UpgradeThread(QObject *parent,
                             const QByteArray &upgradeFileContent,
                             libusb_device *dev) : QThread(parent),
                                                   mFileContent(upgradeFileContent),
                                                   mDevice(dev)
{
    //mFileContent = upgradeFileContent;
    libusb_ref_device(mDevice);
    mCancelled = false;
}

UpgradeThread::~UpgradeThread()
{
    libusb_unref_device(mDevice);
}

void UpgradeThread::run(){
    // step 0: 分析 文件内容
    char magicCode[4];
    memcpy(magicCode, mFileContent.constData(), 4);
    UpgradeProgress progress;
    progress.description = QString::fromUtf8("");
    if( magicCode[0] != 'K' || magicCode[1] != 'E' || magicCode[2] != 'Y' || magicCode[3] != 'U') {
        //ui->upgradeStatus ->setText(QString::fromUtf8("非法文件"));
        progress.state = UPGRADE_FILE_CONTENT_INVALID;
        emit stateChanged(progress);
        return;
    }

    int firmWareLen;
    uint8_t hash[32];
    uint8_t sig[64];

    memcpy(&firmWareLen, mFileContent.constData()+4, 4);
    firmWareLen = ntohl(firmWareLen);

    if( firmWareLen + 108 != mFileContent.size()) {
        //ui->upgradeStatus ->setText(QString::fromUtf8("非法文件"));
        progress.state = UPGRADE_FILE_CONTENT_INVALID;
        emit stateChanged(progress);
        return;
    }

    uint8_t version_major;
    uint8_t version_minor;
    uint16_t version_patch;
    memcpy(&version_major, mFileContent.constData() + 8, 1);
    memcpy(&version_minor, mFileContent.constData() + 9, 1);
    memcpy(&version_patch, mFileContent.constData() + 10, 2);
    version_patch = ntohs(version_patch);
    QString version = QString::fromUtf8("%1.%2.%3")
        .arg(version_major)
        .arg(version_minor)
        .arg(version_patch);

    memcpy(hash, mFileContent.constData() + 12, 32);
    memcpy(sig, mFileContent.constData() + 44, 64);

    // claim usb device
    libusb_device_handle *devHandle;
    int err;
    err = libusb_open(mDevice, &devHandle);
    if (err) {
        progress.state = UPGRADE_USB_IO_ERROR;
        progress.description = QString::fromUtf8("open");
        emit stateChanged(progress);
        return;
    }
    //
    err = libusb_kernel_driver_active(devHandle, 0);
    if (err && err != LIBUSB_ERROR_NOT_SUPPORTED)
    {
        err = libusb_detach_kernel_driver(devHandle, 0);
    }
    if (!err || err == LIBUSB_ERROR_NOT_SUPPORTED)
    {
        err = libusb_claim_interface(devHandle, 0);
    }
    if ( err) {
        libusb_close(devHandle);
        progress.state = UPGRADE_USB_IO_ERROR;
        progress.description = QString::fromUtf8("claim");
        emit stateChanged(progress);
        return;
    }

    int transferred;
    //
    GetModeAndVersionRequest modeVersionRequest;
    uint8_t usb_pkg[1024];
    uint32_t msgType;
    size_t msgLen;

    msgType = MsgTypeGetModeAndVersionRequst;
    msgLen = modeVersionRequest.ByteSizeLong();
    msgType = htonl(msgType);
    msgLen  = htonl(msgLen);
    usb_pkg[0] = 1;
    memcpy(usb_pkg + 1, &msgType, 4);
    memcpy(usb_pkg + 5, &msgLen, 4);
    modeVersionRequest.SerializeToArray(usb_pkg + 9, htonl(msgLen));
#if PROTO_MOCK_TEST == 1
    usb_pkg[0] = 0;
#endif
#if TREZOR_TEST == 0
    err = libusb_bulk_transfer(devHandle, 2, usb_pkg, 1024, &transferred, 2000);
#else
    err = libusb_interrupt_transfer(devHandle, 1, usb_pkg, 1024, &transferred, 2000);
#endif
    if (err || transferred != 1024) {
        libusb_close(devHandle);
        progress.state = UPGRADE_USB_IO_ERROR;
        progress.description = QString::fromUtf8("write %1 %2").arg(err).arg(transferred);
        emit stateChanged(progress);
        return;
    }

    while (!mCancelled) {
#if TREZOR_TEST == 0
        err = libusb_bulk_transfer(devHandle, 129, usb_pkg, 1024, &transferred, 2000);
#else
        err = libusb_interrupt_transfer(devHandle, 129, usb_pkg, 1024, &transferred, 2000);
#endif
        if (err == LIBUSB_ERROR_TIMEOUT) {
            continue;
        }
        if (err || transferred != 1024) {
            libusb_close(devHandle);
            progress.state = UPGRADE_USB_IO_ERROR;
            progress.description = QString::fromUtf8("read %1 %2").arg(err).arg(transferred);
            emit stateChanged(progress);
            return;
        }
        else{
            break;
        }
    }
    // 解析回复
    if( mCancelled) {
        return;
    }
#if PROTO_MOCK_TEST == 1
    if (usb_pkg[0] != 0){ //单个包回复
#else
    if (usb_pkg[0] != 1){ //单个包回复
#endif
        progress.state = UPGRADE_USB_PROTOCOL_ERROR;
        progress.description = QString::fromUtf8("pkg exp %1 real %2").arg(1).arg(usb_pkg[0]);
        emit stateChanged(progress);
        return;
    }
#if PROTO_MOCK_TEST != 1
    memcpy(&msgType, usb_pkg + 1, 4);
    memcpy(&msgLen, usb_pkg + 5, 4);
    msgType = htonl(msgType);
    msgLen = htonl(msgLen);

    if( msgType != MsgTypeGetModeAndVersionReply) {
        libusb_close(devHandle);
        progress.state = UPGRADE_USB_PROTOCOL_ERROR;
        progress.description = QString::fromUtf8("getVersion and Mode");
        emit stateChanged(progress);
        return;

    }
    else {

        GetModeAndVersionReply rep;
        membuf mbuf((char*)usb_pkg+9, (char*)usb_pkg+9+msgLen);
        std::istream stream(&mbuf);
        if( !rep.ParseFromIstream(&stream)){
            libusb_close(devHandle);
            progress.state = UPGRADE_USB_PROTOCOL_ERROR;
            progress.description = QString::fromUtf8("getVersion and Mode");
            emit stateChanged(progress);
            return;
        }
        if( rep.mode() == MODE_APP) {
            libusb_close(devHandle);
            progress.state = UPGRADE_DEVICE_STATE_ERROR;
            progress.description = QString::fromUtf8("Please reboot to bootloader mode");
            emit stateChanged(progress);
            return;
        }


        progress.currentFirmwareVersion = QString::fromUtf8(rep.firmwareversion().c_str() );
        progress.targetFirmwareVersion = version;

    }

#endif

    progress.state = UPGRADE_SENDING_REQUEST;
    emit stateChanged(progress);

    UpgradeStartRequest upStartReq;
    upStartReq.set_firmwareversion(version.toUtf8().constData());
    upStartReq.set_sha256hash((const char*)hash,32);


    usb_pkg[0] = 1; //single pkg
    msgType = MsgTypeUpgradeStartRequest;
    msgLen = upStartReq.ByteSizeLong();
    msgType = htonl(msgType);
    msgLen = htonl(msgLen);
    memcpy(usb_pkg + 1, &msgType, 4);
    memcpy(usb_pkg + 5, &msgLen, 4);
    upStartReq.SerializeToArray(usb_pkg + 9, htonl(msgLen));
#if PROTO_MOCK_TEST == 1
    usb_pkg[0] = 0;
#endif
    #if TREZOR_TEST == 0
        err = libusb_bulk_transfer(devHandle, 2, usb_pkg, 1024, &transferred, 2000);
    #else
        err = libusb_interrupt_transfer(devHandle, 1, usb_pkg, 1024, &transferred, 2000);
    #endif
    if (err || transferred != 1024) {
        libusb_close(devHandle);
        progress.state = UPGRADE_USB_IO_ERROR;
        progress.description = QString::fromUtf8("write %1 %2").arg(err).arg(transferred);
        emit stateChanged(progress);
        return;
    }
    while (!mCancelled) {
#if TREZOR_TEST == 0
        err = libusb_bulk_transfer(devHandle, 129, usb_pkg, 1024, &transferred, 2000);
#else
        err = libusb_interrupt_transfer(devHandle, 129, usb_pkg, 1024, &transferred, 2000);
#endif
        if (err == LIBUSB_ERROR_TIMEOUT) {
            continue;
        }
        if (err || transferred != 1024) {
            libusb_close(devHandle);
            progress.state = UPGRADE_USB_IO_ERROR;
            progress.description = QString::fromUtf8("read %1 %2").arg(err).arg(transferred);
            emit stateChanged(progress);
            return;
        }
        else{
            break;
        }
    }
    // 解析回复
    if( mCancelled) {
        return;
    }
#if PROTO_MOCK_TEST == 1
    if (usb_pkg[0] != 0){ //单个包回复
#else
    if (usb_pkg[0] != 1){ //单个包回复
#endif
        progress.state = UPGRADE_USB_PROTOCOL_ERROR;
        progress.description = QString::fromUtf8("pkg exp %1 real %2").arg(1).arg(usb_pkg[0]);
        emit stateChanged(progress);
        return;
    }
#if PROTO_MOCK_TEST != 1
    memcpy(&msgType, usb_pkg + 1, 4);
    memcpy(&msgLen, usb_pkg + 5, 4);
    msgType = htonl(msgType);
    msgLen = htonl(msgLen);


    if( msgType == MsgTypeRequestRejected) {
        libusb_close(devHandle);
        progress.state = UPGRADE_USER_REJECTED;
        progress.description = QString::fromUtf8(" ");
        emit stateChanged(progress);
        return;
    }

    if( msgType != MsgTypeGenericConfirmReply) {
        libusb_close(devHandle);
        progress.state = UPGRADE_USB_PROTOCOL_ERROR;
        progress.description = QString::fromUtf8("msgtype exp %1 real %2").arg(MsgTypeGenericConfirmReply).arg(msgType);
        emit stateChanged(progress);

        return;
    }
    // 用户已经确认
#endif
    progress.state = UPGRADE_SENDING_CONTENT;
    progress.upgradeFileLen = firmWareLen;
    progress.currentSendOffset = 0;
    emit stateChanged(progress);

    /*
    // 构造消息
    SendUpgradeFirmware sendReq;
    sendReq.set_firmwareversion(version.toUtf8().constData());
    sendReq.set_sha256hash((const char*)hash, 32);
    sendReq.set_signature((const char*)sig, 64);
    sendReq.set_firmware(mFileContent.constData() + 108, firmWareLen);

    msgLen = sendReq.ByteSizeLong();
    char * protoBuffer = (char*)malloc(msgLen);
    sendReq.SerializeToArray(protoBuffer, msgLen);
    */
    msgLen = mFileContent.size();
    // send pkg one by one
    int offset = 0;

    msgType = MsgTypeSendUpgradeFirmware;
    while(offset < msgLen && !mCancelled) {
        int copyLen;
        if( offset == 0){
#if PROTO_MOCK_TEST == 1
            usb_pkg[0] = 0;
#else
            usb_pkg[0] = 6;
#endif
            msgType = htonl(msgType);
            int tempLen = htonl(msgLen);
            // memcpy(usb_pkg + 1, &msgType, 4);
            usb_pkg[1] = 1;
            memcpy(usb_pkg + 5, &tempLen, 4);
            copyLen = 1015;
            memcpy(usb_pkg + 9,  mFileContent.constData(), 1015);
            offset += copyLen;
        }
        else {
#if PROTO_MOCK_TEST == 1
            usb_pkg[0] = 0;
#else
            usb_pkg[0] = 7;
#endif
            int tempOffset = htonl(offset);
            memcpy(usb_pkg + 1, &tempOffset, 4);
            tempOffset = offset + 1019;
            //int copyLen;
            if( tempOffset > msgLen){
                copyLen = msgLen - offset;
            }
            else{
                copyLen = 1019;
            }
            memcpy(usb_pkg + 5, mFileContent.constData() + offset, copyLen);
            offset += copyLen;
        }
        // sendPkg
        #if TREZOR_TEST == 0
            err = libusb_bulk_transfer(devHandle, 2, usb_pkg, 1024, &transferred, 2000);
        #else
            err = libusb_interrupt_transfer(devHandle, 1, usb_pkg, 1024, &transferred, 2000);
        #endif
        if (err || transferred != 1024) {
            libusb_close(devHandle);
            progress.state = UPGRADE_USB_IO_ERROR;
            progress.description = QString::fromUtf8("write %1 %2").arg(err).arg(transferred);
            emit stateChanged(progress);
            return;
        }
        // readPkg
        if (offset < msgLen) {

#if TREZOR_TEST == 0
           err = libusb_bulk_transfer(devHandle, 129, usb_pkg, 1024, &transferred, 3000);
#else
           err = libusb_interrupt_transfer(devHandle, 129, usb_pkg, 1024, &transferred, 3000);
#endif
           if(err || transferred != 1024){
               if( err == LIBUSB_ERROR_TIMEOUT ) {
                   qDebug() << "timeout " << offset << "\n";
                   // resend current package again.
                   offset -= copyLen;
                   continue;
               }
               libusb_close(devHandle);
               progress.state = UPGRADE_USB_IO_ERROR;
               progress.description = QString::fromUtf8("read %1 %2").arg(err).arg(transferred);
               emit stateChanged(progress);
               return;
           }

        // the pkg must be ack
#if PROTO_MOCK_TEST != 1
            int readOffset;
            memcpy(&readOffset, usb_pkg+1, 4);
            readOffset = ntohl(readOffset);
            if( usb_pkg[0] != 4 || readOffset != offset) {
                libusb_close(devHandle);
                progress.state = UPGRADE_USB_PROTOCOL_ERROR;
                progress.description = QString::fromUtf8("%1 != %2 or %3 != %4")
                        .arg(4)
                        .arg(usb_pkg[0])
                        .arg(offset)
                        .arg(readOffset)
                        ;
                emit stateChanged(progress);
                return;
            }
 #endif
        }
        progress.currentSendOffset = offset;
        emit stateChanged(progress);
        // next round
    }
    if( mCancelled) {
        libusb_close(devHandle);
        return;
    }
    // 读取最后的包
    progress.state = UPGRADE_SENT_FINISHED;
    emit stateChanged(progress);
    while (!mCancelled) {
#if TREZOR_TEST == 0
        err = libusb_bulk_transfer(devHandle, 129, usb_pkg, 1024, &transferred, 5000);
#else
        err = libusb_interrupt_transfer(devHandle, 129, usb_pkg, 1024, &transferred, 5000);
#endif  
        if (err == LIBUSB_ERROR_TIMEOUT || err == LIBUSB_ERROR_IO) {
            break;
        }
        if (err || transferred != 1024) {
            if( err != LIBUSB_ERROR_NOT_FOUND){
                libusb_close(devHandle);
            }
            progress.state = UPGRADE_USB_IO_ERROR;
            progress.description = QString::fromUtf8("read %1 %2").arg(err).arg(transferred);
            emit stateChanged(progress);
            return;
        }
        else{
            break;
        }
    }

    // 解析回复
    if( mCancelled) {
        return;
    }
  #if PROTO_MOCK_TEST != 1
    if( err == LIBUSB_ERROR_TIMEOUT || err == LIBUSB_ERROR_IO) {
        libusb_close(devHandle);
        progress.state = UPGRADE_WAITING_DEVICE_WRITE;
        emit stateChanged(progress);
        return;
    }
    if (usb_pkg[0] != 1){ //单个包回复
        progress.state = UPGRADE_USB_PROTOCOL_ERROR;
        progress.description = QString::fromUtf8("pkg exp %1 real %2").arg(1).arg(usb_pkg[0]);
        emit stateChanged(progress);
        return;
    }
    memcpy(&msgType, usb_pkg + 1, 4);
    memcpy(&msgLen, usb_pkg + 5, 4);
    msgType = htonl(msgType);
    msgLen = htonl(msgLen);

    if( msgType == MsgTypeRequestRejected) {
        libusb_close(devHandle);
        progress.state = UPGRADE_USER_REJECTED;
        // progress.description = QString::fromUtf8(" ");
        progress.description = QString::fromUtf8("firmware error.");
        emit stateChanged(progress);
        return;
    }

    if( msgType != MsgTypeGenericConfirmReply) {
        libusb_close(devHandle);
        progress.state = UPGRADE_USB_PROTOCOL_ERROR;
        progress.description = QString::fromUtf8("msgtype exp %1 real %2").arg(MsgTypeGenericConfirmReply).arg(msgType);
        emit stateChanged(progress);

        return;
    }
 #endif
    progress.state = UPGRADE_FINISHED;
    emit stateChanged(progress);
    libusb_close(devHandle);

}
