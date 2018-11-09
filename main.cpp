#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QFont font("PT Sans");
    font.setStyleHint(QFont::SansSerif);
    font.setPointSize(15);
    font.setPixelSize(15);
    

    QApplication a(argc, argv);
    QApplication::setFont(font);
    MainWindow w;
    w.show();

    return a.exec();
}
