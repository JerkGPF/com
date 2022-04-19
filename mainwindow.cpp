#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->serialPort = new QSerialPort;
    findFreePorts();
    ui->writeDB->hide();//将写入数据库隐藏
    //串口打开
    connect(ui->openCom, &QCheckBox::toggled, [=](bool checked){
        if (checked){
            initSerialPort();
            dataTimer.start(0);
            ui->writeDB->show();
        }else{
            this->serialPort->close();
            ui->openCom->setChecked(false);
            dataTimer.stop();

            dbTimer.stop();
            db.close();
            ui->writeDB->setCheckState(Qt::Unchecked);
            ui->writeDB->hide();
        }
    });
    //写入数据库
    connect(ui->writeDB,&QCheckBox::toggled,[=](bool checked){
        if(checked){
            if(db.isOpen())
            {
                dbTimer.start(3000);
            }else {
                db.open();
                dbTimer.start(3000);
            }
        }
        else
        {
            dbTimer.stop();
            db.close();
        }
    });
    connect(this->serialPort, SIGNAL(readyRead()), this, SLOT(recvMsg()));
    connect(&dbTimer, &QTimer::timeout, [=](){
        insertDB(msg);
    });
    openDatabase();//打开数据库连接
    setupRealtimeDataDemo(ui->customPlot);//图表初始化
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupRealtimeDataDemo(QCustomPlot *customPlot)
{
    QCustomPlot *dynamic = customPlot;
    dynamic->addGraph();  //添加图形一
    dynamic->graph(0)->setPen(QPen(QColor(255, 0, 0)));

    dynamic->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5));//散点

    dynamic->yAxis->setRange(-10, 30); //设置y轴的范围
    //坐标轴使用时间刻度
    int showTime=1;//1秒

    //设置现在最新时间
    QDateTime dateTime = QDateTime::currentDateTime();
    double now = dateTime.toTime_t();

    QSharedPointer<QCPAxisTickerDateTime> timer(new QCPAxisTickerDateTime());

    timer->setTickCount(5);

    timer->setDateTimeFormat("hh:mm:ss:zzz");

    timer->setTickStepStrategy(QCPAxisTicker::tssMeetTickCount);

    dynamic->xAxis->setTicker(timer);

    customPlot->xAxis->setRange(now-showTime,now+showTime);


    customPlot->xAxis->setTickLabelRotation(30);//设置x轴时间旋转角度为30度

    dynamic->axisRect()->setupFullAxesBox();



    // 使上下轴、左右轴范围同步
    connect(dynamic->xAxis, SIGNAL(rangeChanged(QCPRange)), dynamic->xAxis2, SLOT(setRange(QCPRange)));
    connect(dynamic->yAxis, SIGNAL(rangeChanged(QCPRange)), dynamic->yAxis2, SLOT(setRange(QCPRange)));

    connect(&dataTimer, SIGNAL(timeout()), this, SLOT(realtimeDataSlot()));

    //使用鼠标拖动轴范围，使用鼠标滚轮缩放，并通过单击选择图形：
    dynamic->setInteractions(QCP::iRangeDrag |
                             QCP::iRangeZoom | QCP::iSelectPlottables);

}

//寻找空闲状态串口
void MainWindow::findFreePorts(){
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (int i = 0; i < ports.size(); ++i){
        if (ports.at(i).isBusy()){
            ports.removeAt(i);
            continue;
        }
        ui->portName->addItem(ports.at(i).portName());
    }
    if (!ports.size()){
        QMessageBox::warning(NULL,"Tip",QStringLiteral("找不到空闲串口"));
        return;
    }
}
//初始化串口
bool MainWindow::initSerialPort(){
    this->serialPort->setPortName(ui->portName->currentText());
    if (!this->serialPort->open(QIODevice::ReadWrite)){
        QMessageBox::warning(NULL,"Tip",QStringLiteral("串口打开失败"));
        return false;
    }
    this->serialPort->setBaudRate(ui->baudRate->currentText().toInt());

    if (ui->dataBits->currentText().toInt() == 8){
        this->serialPort->setDataBits(QSerialPort::Data8);
    }else if (ui->dataBits->currentText().toInt() == 7){
        this->serialPort->setDataBits(QSerialPort::Data7);
    }else if (ui->dataBits->currentText().toInt() == 6){
        this->serialPort->setDataBits(QSerialPort::Data6);
    }else if (ui->dataBits->currentText().toInt() == 5){
        this->serialPort->setDataBits(QSerialPort::Data5);
    }

    if (ui->stopBits->currentText().toInt() == 1){
        this->serialPort->setStopBits(QSerialPort::OneStop);
    }else if (ui->stopBits->currentText().toInt() == 2){
        this->serialPort->setStopBits(QSerialPort::TwoStop);
    }


    if(ui->parity->currentText() == "NoParity"){
        this->serialPort->setParity(QSerialPort::NoParity);
    }else if (ui->parity->currentText() == "EvenParity"){
        this->serialPort->setParity(QSerialPort::EvenParity);
    }else if (ui->parity->currentText() == "OddParity"){
        this->serialPort->setParity(QSerialPort::OddParity);
    }
    return true;
}
//向串口发送信息
void MainWindow::sendMsg(const QString &msg){
    this->serialPort->write(QByteArray::fromHex(msg.toLatin1()));
    ui->comLog->insertPlainText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " [send] " + msg + "\n");
}
//接受来自串口的信息
void MainWindow::recvMsg(){
    static QByteArray sumData;//用于缓存所有数据
    static int clearNum=0;//首次判断计数
    QString tempData;
    QStringList list_1;
    QByteArray buffer=this->serialPort->readAll();
    if(!buffer.isEmpty())//如果有数据发送到上位机
    {
        sumData.append(buffer);//因为每次数据传输不完整且长度不固定，此方法append用于确保将每帧数据都接收完整后，再进行处理判断
        //如果数据中包含了帧头("0xff")和帧尾("0xfd")
        //此处帧头和帧尾由自己确定
        if(sumData.contains("0xff") and sumData.contains("0xfd"))
        {
            //必须确保这是一帧完整的数据，即("0xff")一定要在最前面。
            //一帧错误的数据如：2.0000,3.0000,0xfd,0xff 也是包含了帧头和帧尾的
            //所以此处是确保数据格式严格为："0xff,21.149988,0xfd"
            if(sumData.indexOf("0xff")==0)//oxff为帧头
            {
                //此处是为了提高软件兼容性
                //因为可能在没有打开串口前，串口缓存区已经接收到下位机的大量不完整数据：
                //",21.170008,20.809992,0xfd0xff,21.050013,0xfd"
                //对于这种情况，第一次打开串口就先把缓存区的数据全部清空，再重新接收
                if(clearNum==0)
                {
                    buffer.clear();
                    sumData.clear();
                    clearNum=1;
                }
                else
                {
                    //例如对于这样一串数据（已经包含帧头和帧尾），但不完整：
                    //"0xff,21.050013,0xfd0xff,21.1"
                    //先找到帧尾的位置，并向前截断，得到这样的数据"0xff,21.050013,"
                    //这串数据的大小为15
                    // sumData.left(4) 表示取前4个字符即得到0xff
                    // sumData.right(15 - 4 - 1) 表示取后10个字符即得到 21.050013,
                    qDebug()<<"sumData:"<<sumData;
                    int nIndex_1 = sumData.indexOf("0xfd");     //表示"0xfd"之前有nIndex位
                    qDebug()<<"nIndex_1:"<<nIndex_1;
                    sumData = sumData.left(nIndex_1);

                    //再找到帧头的位置，并往后截断，从而严格保证得到我们想要的数据：",21.050013,"
                    //数据中间有 ， 隔断
                    int nIndex_2 = sumData.indexOf("0xff");     //表示"0xff"之前有nIndex位
                    qDebug()<<"nIndex_2:"<<nIndex_2;
                    sumData = sumData.right(nIndex_1 - nIndex_2 - 4);      // 取后nIndex位
                    qDebug()<<"sumData:"<<sumData;
                    qDebug()<<"sumData.size():"<<sumData.size();
                    //sumdata不能直接处理，先转化为字符串格式，并用split函数进行分割
                    //",21.050013,,"-->" NULL , 21.050013  , NULL "这样的数据分割后会得到3个数据
                    //list_1.at(0/2):-->NULL  list_1.at(1):--> 21.050013  list_1.at(2):--> NULL
                    tempData = sumData;
                    list_1 = tempData.split(",");
                    qDebug()<<"list_1.at(1):"<<list_1.at(1);
                    if(list_1.length()==3)
                    {
                        msg = list_1.at(1).toStdString().c_str();
                        qDebug()<<"msg"<<msg;
                        //                        bool ret = insertDB(msg);
                        //                        qDebug()<<"insertDB Status:"<<ret;
                        ui->comLog->insertPlainText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " [recieve] " + msg + "\n");
                    }
                }
            }
            //如果帧头不是"0xff"，即数据可能是 :
            //sumData: ",20.929987,0xff,21.330011,0xfd"
            //不用将数据清空，直接从0xff往右截断
            else
            {
                int nIndex_3 = sumData.indexOf("0xff");
                sumData = sumData.right(sumData.size() - nIndex_3);
                buffer.clear();
            }
        }
    }

}

void MainWindow::openDatabase()
{
    db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName("127.0.0.1");
    db.setPort(3306);
    db.setDatabaseName("temperature");
    db.setUserName("root");
    db.setPassword("root1996");
    if(!db.open())
    {
        QMessageBox::warning(this,"错误","数据库打开失败 !");
    }
    else
    {
        QMessageBox::warning(this,"成功","数据库打开成功!");
        qDebug()<<"database opened success!";
    }
    query=QSqlQuery(db);//让数据库关联为db
    query.exec("DROP TABLE IF EXISTS tempTest");
    query.exec("CREATE TABLE IF NOT EXISTS tempTest(CurrentTime TEXT ,Temperature TEXT);");
}

void MainWindow::realtimeDataSlot()
{
    QCustomPlot *dynamic = ui->customPlot;

    static QTime time(QTime::currentTime());
    double key = time.elapsed()/1000.0; // 开始到现在的时间，单位秒
    //qDebug() << key;
    //时间
    //时间
    static double lastKey = 0;
    if (key - lastKey > 0.5) // 大约500ms添加一次数据
    {
        //添加数据到graph
        dynamic->graph(0)->addData(key, msg.toDouble());
        lastKey = key;  //记录当前时刻
    }

    dynamic->legend->setVisible(true);  //图例可见
    dynamic->graph(0)->setName(tr("温度"));  //设置线名

    dynamic->xAxis->setLabel("x 时间");   // 设置x轴的标签
    dynamic->yAxis->setLabel("y 温度");   // 设置y轴的标签
    // 曲线能动起来的关键
    dynamic->xAxis->setRange(key, 8, Qt::AlignRight);
    //重绘图
    dynamic->replot();
}

//插入数据至数据库，连续相同数据不存储
bool MainWindow::insertDB(QByteArray msg)
{
    static QByteArray tempData;
    if(tempData==msg)
    {
        return false;
    }
    else
    {
        tempData = msg;
        QString ordertime = QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss");//当前时间
        query=QSqlQuery(db);//让数据库关联为db
        query.prepare("insert into tempTest(CurrentTime,Temperature) values(?,?);");
        //给字段设置内容 list
        QVariantList timeList;
        timeList<<ordertime;
        QVariantList dataList;
        dataList<<msg;
        //给字段绑定相应的值，按顺序绑定
        query.addBindValue(timeList);
        query.addBindValue(dataList);
        //执行预处理命令
        return query.execBatch();
    }
}



