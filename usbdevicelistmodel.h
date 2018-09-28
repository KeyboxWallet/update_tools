#ifndef USBDEVICELISTMODEL_H
#define USBDEVICELISTMODEL_H

#include <QAbstractItemModel>

#include "libusb.h"
class UsbDeviceListModel : public QAbstractListModel
{
public:
    UsbDeviceListModel();
    virtual ~UsbDeviceListModel();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    libusb_device *getDevice(int index);

    void setAllData(libusb_device *usb_devs[], int count);
private :
    libusb_device ** mDevices;
    int mDevCount;
};

#endif // USBDEVICELISTMODEL_H
