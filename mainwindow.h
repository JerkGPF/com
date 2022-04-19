#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "qcustomplot.h"

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>

#include <QList>
#include <QMessageBox>
#include <QDateTime>
#include <QSqlQuery>
#include <QTimer>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    //设置qcustomplot画图属性，实时
    void setupRealtimeDataDemo(QCustomPlot *customPlot);


protected:
    void findFreePorts();
    bool initSerialPort();
    void sendMsg(const QString &msg);

public slots:
    void recvMsg();

    void openDatabase();

    //添加实时数据槽
    void realtimeDataSlot();

private:
    Ui::MainWindow *ui;

    QSerialPort *serialPort;

    QTimer dataTimer;
    QTimer dbTimer;

    QSqlQuery query ;
    QSqlDatabase db;
    QByteArray msg;

    bool insertDB(QByteArray msg);



public:
    int changefromHex_to_ascii(QString str);
};

#endif // MAINWINDOW_H
