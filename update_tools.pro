#-------------------------------------------------
#
# Project created by QtCreator 2018-07-24T11:27:02
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = update_tools
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += libusb-1.0
}

win32{
 QMAKE_LIBS += ws2_32.lib shell32.lib
 QMAKE_CXXFLAGS += -utf-8
 if (!exists ($$(vcpkg_root)/installed/x86-windows/include/libusb-1.0) ) {
  error("please install libusb var vcpkg and set vcpkg_root environment variable to your vcpkg dir.")
 }
 QMAKE_CXXFLAGS += -D_WIN32_WINNT=0x0601 -I$$(vcpkg_root)/installed/x86-windows/include/libusb-1.0
 debug {
   QMAKE_LIBS += $$(vcpkg_root)/installed/x86-windows/debug/lib/libusb-1.0.lib
 }
 release {
   QMAKE_LIBS += $$(vcpkg_root)/installed/x86-windows/lib/libusb-1.0.lib
 }
}

# workaround for qtmain.lib link issue.
# https://github.com/Microsoft/vcpkg/issues/1442
win32 {
   debug {
    QMAKE_LIBDIR += $$(vcpkg_root)/installed/x86-windows/debug/lib/manual-link
  }
  release {
    QMAKE_LIBDIR += $$(vcpkg_root)/installed/x86-windows/lib/manual-link
  }
}

SOURCES += \
        main.cpp \
        mainwindow.cpp \
    usbdevicelistmodel.cpp

HEADERS += \
        mainwindow.h \
    usbdevicelistmodel.h

FORMS += \
        mainwindow.ui
