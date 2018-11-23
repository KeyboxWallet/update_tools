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


bool UpgradeThread::waitDeviceConnect()
{

    mProgress.state = UPGRADE_WAIT_USB_DEVICE;
    emit stateChanged(mProgress);
    int devCnt = 0;
    while( (devCnt = scanUsbDevice()) == 0){
        QThread::sleep(1);
    }
    if (devCnt >= 2) {
        mProgress.state = UPGRADE_USB_DEVICE_TOO_MANY;
        emit stateChanged(mProgress);
        return false;
    }
    // devCnt == 1,
    QThread::sleep(2);
    return checkDeviceMode();

    //return true;
}


int UpgradeThread::scanUsbDevice()
{
    int dev_cnt = 0;
    libusb_device * keybox_devices[2];

    libusb_device **list;
    ssize_t cnt = libusb_get_device_list(NULL, &list);

    libusb_device *device;
    struct libusb_device_descriptor descriptor;

    //UsbDevice * usb_devices

    for(int i=0; i<cnt && dev_cnt < 2; i++){
        device = list[i];
        int err = libusb_get_device_descriptor(device, &descriptor);

        if (err) {
            continue;
        }
        if (    descriptor.idVendor == KEYBOX2_VENDOR_ID
            &&  descriptor.idProduct == KEYBOX2_PRODUCT_ID
            &&  descriptor.bcdDevice == KEYBOX2_BCD_DEVICE
            ) {
                keybox_devices[dev_cnt++] = device;
        }
    }

    if( dev_cnt == 1){

        mDevice = libusb_ref_device(keybox_devices[0]);
        //return false;
    }
    libusb_free_device_list(list, true);
    return dev_cnt;

}

bool UpgradeThread::checkDeviceMode()
{

    int err = libusb_open(mDevice, &mHandle);
    int transferred;
    if( err ) {
        mHandle = NULL;

        mProgress.state = UPGRADE_USB_IO_ERROR;
        mProgress.description = QString::fromUtf8("open %1 ").arg(err);
        emit stateChanged(mProgress);
        return false;
    }

    err = libusb_kernel_driver_active(mHandle, 0);
    if (err && err != LIBUSB_ERROR_NOT_SUPPORTED)
    {
        err = libusb_detach_kernel_driver(mHandle, 0);
    }
    if (!err || err == LIBUSB_ERROR_NOT_SUPPORTED)
    {
        err = libusb_claim_interface(mHandle, 0);
    }
    if ( err) {
        cleanup();
        mProgress.state = UPGRADE_USB_IO_ERROR;
        mProgress.description = QString::fromUtf8("claim");
        emit stateChanged(mProgress);
        return false;
    }

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
    err = libusb_bulk_transfer(mHandle, 2, usb_pkg, 1024, &transferred, 2000);
    if (err || transferred != 1024) {
        mProgress.state = UPGRADE_USB_IO_ERROR;
        mProgress.description = QString::fromUtf8("write %1 %2").arg(libusb_error_name(err)).arg(transferred);
        emit stateChanged(mProgress);
        return false;
    }

    // read
    err = libusb_bulk_transfer(mHandle, 129, usb_pkg, 1024, &transferred, 2000);
    if (err || transferred != 1024) {
        mProgress.state = UPGRADE_USB_IO_ERROR;
        mProgress.description = QString::fromUtf8("read %1 %2").arg(libusb_error_name(err)).arg(transferred);
        emit stateChanged(mProgress);
        return false;
    }

    memcpy(&msgType, usb_pkg + 1, 4);
    memcpy(&msgLen, usb_pkg + 5, 4);
    msgType = htonl(msgType);
    msgLen = htonl(msgLen);

    if( msgType != MsgTypeGetModeAndVersionReply) {
        mProgress.state = UPGRADE_USB_PROTOCOL_ERROR;
        mProgress.description = QString::fromUtf8("getVersionAndMode");
        emit stateChanged(mProgress);
        return false;

    }
    else {

        GetModeAndVersionReply rep;
        membuf mbuf((char*)usb_pkg+9, (char*)usb_pkg+9+msgLen);
        std::istream stream(&mbuf);
        if( !rep.ParseFromIstream(&stream)){
            mProgress.state = UPGRADE_USB_PROTOCOL_ERROR;
            mProgress.description = QString::fromUtf8("getVersionAndMode");
            emit stateChanged(mProgress);
            return false;
        }
        if( rep.mode() == MODE_APP) {
            mProgress.state = UPGRADE_DEVICE_STATE_ERROR;
            mProgress.description = QString::fromUtf8("Please reboot to bootloader mode");
            emit stateChanged(mProgress);
            return false;
        }
        mProgress.currentFirmwareVersion = QString::fromUtf8(rep.firmwareversion().c_str() );

   }
   return true;
}


void UpgradeThread::cleanup()
{
    if( mHandle){
        libusb_close(mHandle);
        mHandle = NULL;
    }
    if( mDevice ) {
        libusb_unref_device(mDevice);
        mDevice = NULL;
    }
}

UpgradeThread::UpgradeThread(QObject *parent,
                             const QByteArray &upgradeFileContent
                             ) : QThread(parent),
                    mFileContent(upgradeFileContent)

{
    //mFileContent = upgradeFileContent;
    mCancelled = false;
    mHandle = NULL;
    mDevice = NULL;
}

UpgradeThread::~UpgradeThread()
{
    cleanup();
}

void UpgradeThread::run(){
    // step 0: 分析 文件内容
    char magicCode[4];
    memcpy(magicCode, mFileContent.constData(), 4);
    mProgress.description = QString::fromUtf8("");
    if( magicCode[0] != 'K' || magicCode[1] != 'E' || magicCode[2] != 'Y' || magicCode[3] != 'U') {
        //ui->upgradeStatus ->setText(QString::fromUtf8("非法文件"));
        mProgress.state = UPGRADE_FILE_CONTENT_INVALID;
        emit stateChanged(mProgress);
        return;
    }

    int firmWareLen;
    uint8_t hash[32];
    uint8_t sig[64];

    memcpy(&firmWareLen, mFileContent.constData()+4, 4);
    firmWareLen = ntohl(firmWareLen);

    if( firmWareLen + 108 != mFileContent.size()) {
        //ui->upgradeStatus ->setText(QString::fromUtf8("非法文件"));
        mProgress.state = UPGRADE_FILE_CONTENT_INVALID;
        emit stateChanged(mProgress);
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

    mDevice = NULL;
    mHandle = NULL;
    if(!waitDeviceConnect()){
        cleanup();
        return;
    }

    mProgress.targetFirmwareVersion = version;
    mProgress.state = UPGRADE_SENDING_REQUEST;
    emit stateChanged(mProgress);

    //UpgradeProgress mProgress;
    uint8_t usb_pkg[1024];
    uint32_t msgType;
    size_t msgLen;
    int err,transferred;


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
        err = libusb_bulk_transfer(mHandle, 2, usb_pkg, 1024, &transferred, 2000);
    #else
        err = libusb_interrupt_transfer(mHandle, 1, usb_pkg, 1024, &transferred, 2000);
    #endif
    if (err || transferred != 1024) {
        cleanup();
        mProgress.state = UPGRADE_USB_IO_ERROR;
        mProgress.description = QString::fromUtf8("write %1 %2").arg(err).arg(transferred);
        emit stateChanged(mProgress);
        return;
    }
    while (!mCancelled) {
#if TREZOR_TEST == 0
        err = libusb_bulk_transfer(mHandle, 129, usb_pkg, 1024, &transferred, 2000);
#else
        err = libusb_interrupt_transfer(mHandle, 129, usb_pkg, 1024, &transferred, 2000);
#endif
        if (err == LIBUSB_ERROR_TIMEOUT) {
            continue;
        }
        if (err || transferred != 1024) {
            cleanup();
            mProgress.state = UPGRADE_USB_IO_ERROR;
            mProgress.description = QString::fromUtf8("read %1 %2").arg(err).arg(transferred);
            emit stateChanged(mProgress);
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
        mProgress.state = UPGRADE_USB_PROTOCOL_ERROR;
        mProgress.description = QString::fromUtf8("pkg exp %1 real %2").arg(1).arg(usb_pkg[0]);
        emit stateChanged(mProgress);
        return;
    }
#if PROTO_MOCK_TEST != 1
    memcpy(&msgType, usb_pkg + 1, 4);
    memcpy(&msgLen, usb_pkg + 5, 4);
    msgType = htonl(msgType);
    msgLen = htonl(msgLen);


    if( msgType == MsgTypeRequestRejected) {
        cleanup();
        mProgress.state = UPGRADE_USER_REJECTED;
        mProgress.description = QString::fromUtf8(" ");
        emit stateChanged(mProgress);
        return;
    }

    if( msgType != MsgTypeGenericConfirmReply) {
        cleanup();
        mProgress.state = UPGRADE_USB_PROTOCOL_ERROR;
        mProgress.description = QString::fromUtf8("msgtype exp %1 real %2").arg(MsgTypeGenericConfirmReply).arg(msgType);
        emit stateChanged(mProgress);

        return;
    }
    // 用户已经确认
#endif
    mProgress.state = UPGRADE_SENDING_CONTENT;
    mProgress.upgradeFileLen = firmWareLen;
    mProgress.currentSendOffset = 0;
    emit stateChanged(mProgress);

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
            err = libusb_bulk_transfer(mHandle, 2, usb_pkg, 1024, &transferred, 2000);
        #else
            err = libusb_interrupt_transfer(mHandle, 1, usb_pkg, 1024, &transferred, 2000);
        #endif
        if (err || transferred != 1024) {
            cleanup();
            mProgress.state = UPGRADE_USB_IO_ERROR;
            mProgress.description = QString::fromUtf8("write %1 %2").arg(err).arg(transferred);
            emit stateChanged(mProgress);
            return;
        }
        // readPkg
        if (offset < msgLen) {

#if TREZOR_TEST == 0
           err = libusb_bulk_transfer(mHandle, 129, usb_pkg, 1024, &transferred, 3000);
#else
           err = libusb_interrupt_transfer(mHandle, 129, usb_pkg, 1024, &transferred, 3000);
#endif
           if(err || transferred != 1024){
               if( err == LIBUSB_ERROR_TIMEOUT ) {
                   qDebug() << "timeout " << offset << "\n";
                   // resend current package again.
                   offset -= copyLen;
                   continue;
               }
               cleanup();
               mProgress.state = UPGRADE_USB_IO_ERROR;
               mProgress.description = QString::fromUtf8("read %1 %2").arg(err).arg(transferred);
               emit stateChanged(mProgress);
               return;
           }

        // the pkg must be ack
#if PROTO_MOCK_TEST != 1
            int readOffset;
            memcpy(&readOffset, usb_pkg+1, 4);
            readOffset = ntohl(readOffset);
            if( usb_pkg[0] != 4 || readOffset != offset) {
                cleanup();
                mProgress.state = UPGRADE_USB_PROTOCOL_ERROR;
                mProgress.description = QString::fromUtf8("%1 != %2 or %3 != %4")
                        .arg(4)
                        .arg(usb_pkg[0])
                        .arg(offset)
                        .arg(readOffset)
                        ;
                emit stateChanged(mProgress);
                return;
            }
 #endif
        }
        mProgress.currentSendOffset = offset;
        emit stateChanged(mProgress);
        // next round
    }
    if( mCancelled) {
        cleanup();
        return;
    }
    // 读取最后的包
    mProgress.state = UPGRADE_SENT_FINISHED;
    emit stateChanged(mProgress);
    while (!mCancelled) {
#if TREZOR_TEST == 0
        err = libusb_bulk_transfer(mHandle, 129, usb_pkg, 1024, &transferred, 5000);
#else
        err = libusb_interrupt_transfer(mHandle, 129, usb_pkg, 1024, &transferred, 5000);
#endif  
        if (err == LIBUSB_ERROR_TIMEOUT || err == LIBUSB_ERROR_IO) {
            break;
        }
        if (err || transferred != 1024) {
            if( err != LIBUSB_ERROR_NOT_FOUND){
                cleanup();
            }
            mProgress.state = UPGRADE_USB_IO_ERROR;
            mProgress.description = QString::fromUtf8("read %1 %2").arg(err).arg(transferred);
            emit stateChanged(mProgress);
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
        cleanup();
        mProgress.state = UPGRADE_WAITING_DEVICE_WRITE;
        emit stateChanged(mProgress);
        return;
    }
    if (usb_pkg[0] != 1){ //单个包回复
        mProgress.state = UPGRADE_USB_PROTOCOL_ERROR;
        mProgress.description = QString::fromUtf8("pkg exp %1 real %2").arg(1).arg(usb_pkg[0]);
        emit stateChanged(mProgress);
        return;
    }
    memcpy(&msgType, usb_pkg + 1, 4);
    memcpy(&msgLen, usb_pkg + 5, 4);
    msgType = htonl(msgType);
    msgLen = htonl(msgLen);

    if( msgType == MsgTypeRequestRejected) {
        cleanup();
        mProgress.state = UPGRADE_USER_REJECTED;
        // mProgress.description = QString::fromUtf8(" ");
        mProgress.description = QString::fromUtf8("firmware error.");
        emit stateChanged(mProgress);
        return;
    }

    if( msgType != MsgTypeGenericConfirmReply) {
        cleanup();
        mProgress.state = UPGRADE_USB_PROTOCOL_ERROR;
        mProgress.description = QString::fromUtf8("msgtype exp %1 real %2").arg(MsgTypeGenericConfirmReply).arg(msgType);
        emit stateChanged(mProgress);

        return;
    }
 #endif
    mProgress.state = UPGRADE_FINISHED;
    emit stateChanged(mProgress);
    cleanup();

}
