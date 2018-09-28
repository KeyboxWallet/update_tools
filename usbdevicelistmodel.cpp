#include "usbdevicelistmodel.h"

QString getUsbDeviceId(libusb_device *device){
    //mDevice = libusb_ref_device(device);
    // devId += "1";
    struct libusb_device_descriptor descriptor;
    int err = libusb_get_device_descriptor(device, &descriptor);
    if (err)
    {
        //todo: fatal
        return QString::fromUtf8("error");
    }



    return QString::fromUtf8("%1_%2_%3_%4_%5_%6").arg(
                (int)libusb_get_bus_number(device)).arg(
                (int)libusb_get_port_number(device)).arg(
                (int)libusb_get_device_address(device)).arg(
                (int)descriptor.idVendor).arg(
                (int)descriptor.idProduct).arg(
                (int)descriptor.bcdDevice
                );
}


UsbDeviceListModel::UsbDeviceListModel()
{
    mDevCount = 0;
    mDevices = NULL;

}

void UsbDeviceListModel::setAllData(libusb_device **devList, int count)
{
  for(int i=0; i<mDevCount; i++){
    libusb_unref_device(mDevices[i]);
  }
   mDevCount = count;
   if(count > 0){
       mDevices = (libusb_device **)malloc( sizeof(libusb_device *) * count);
       for (int i=0; i<count; i++){
           mDevices[i] = devList[i];
       }
   }
   else{
       mDevices = NULL;
   }
}

UsbDeviceListModel::~UsbDeviceListModel()
{
    if(mDevices){
        free(mDevices);
    }
}


int UsbDeviceListModel::rowCount(const QModelIndex &parent) const
{
    if( parent.isValid() ){
        return 0;
    }
    else
        return mDevCount;
}

QVariant UsbDeviceListModel::data(const QModelIndex &index, int role) const
{
    if(index.row() < mDevCount ){
        if(role == Qt::DisplayRole){
            return getUsbDeviceId(mDevices[index.row()]);
        }
    }
    return QVariant();
}

libusb_device * UsbDeviceListModel::getDevice(int index)
{
    if( index >=0 && index < mDevCount){
        return mDevices[index];
    }
    return NULL;
}
