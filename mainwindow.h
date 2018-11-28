#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "upgradeThread.h"
#include <QtNetwork>




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

private slots:
    void downloadFinished(QNetworkReply* rep);
    void downloadError(QNetworkReply::NetworkError code);


private:
    Ui::MainWindow *ui;
    void selectUpgradeFile();
    void upgrade();
    void downloadUpgradeFile();

    QString mUpgradeFilePath;
    UpgradeThread *mThread;
    QNetworkAccessManager mNetworkManager;
    QNetworkReply *jsonReply;
    QNetworkReply *firmwareReply;

};

#endif // MAINWINDOW_H
