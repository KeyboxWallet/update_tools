#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QFont font("PT Sans");
    font.setStyleHint(QFont::SansSerif);
    font.setPointSize(15);
    font.setPixelSize(15);
    

    QApplication a(argc, argv);
    QApplication::setFont(font);
    QTranslator translator;
    auto appPath = QCoreApplication::applicationDirPath();
    auto macPath = appPath + "/../Resources";
    QString translateFileName = "update_tools_la";
    auto result = translator.load(QLocale(), translateFileName, ".", appPath) ||
        translator.load(QLocale(), translateFileName, ".", macPath);
    a.installTranslator(&translator);

    MainWindow w;
    w.show();

    return a.exec();
}
