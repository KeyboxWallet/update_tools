#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "upgradeThread.h"





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
    void selectUpgradeFile();
    void upgrade();

    QString mUpgradeFilePath;
    UpgradeThread *mThread;

};

#endif // MAINWINDOW_H
