#ifndef ARDUINOSERIAL_H
#define ARDUINOSERIAL_H

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QQueue>
#include <QTimer>

class ArduinoSerial : public QSerialPort
{
    Q_OBJECT
    bool sizeReaded, inputOk, writing, initOk, initTimeout, initMaxSize;
    int chunkSize, maxBufferSize;
    qint64 nextBlockSize;
    int remainingRead, retryCount;
    QQueue <QByteArray> blocks;
    QByteArray buffer, blockLeft;
    qint64 timeout;
    char replyOk[2];
    char zero[2];
    char aliveData[2];
    QTimer aliveTimer;

public:
    explicit ArduinoSerial(QObject *parent = nullptr);
    explicit ArduinoSerial(const QSerialPortInfo &serialPortInfo, QObject *parent = nullptr);
    explicit ArduinoSerial(const QString &name, QObject *parent = nullptr);
    ~ArduinoSerial();
    void openStandart(QSerialPort::BaudRate baud_Rate = Baud9600, QSerialPort::DataBits dataBits = Data8,
                      QSerialPort::Parity aParity = NoParity, QSerialPort::StopBits stopBits = OneStop,
                      QSerialPort::FlowControl flow_Control = NoFlowControl, int onErrorRetryCount = 5);
    void setTimeout(qint64 timeMS = 5000);
    qint64 write(const char *data, qint64 maxSize);
    qint64 write(const QByteArray &byteArray);

    bool isBlockReady(const QByteArray &firstSymbolsOfWantedBlock = QByteArray());
    QByteArray nextBlock(bool alwaysTimeout = false, const QByteArray &firstSymbolsOfWantedBlock = QByteArray());
    void sleep(int msecs);

private:
    quint16 getQuint16(QByteArray arr);
    QByteArray getDataFromQuint16(quint16 val);
    int blockPos(const QByteArray &firstSymbolsOfWantedBlock);

public slots:

private slots:
    void onReadyRead();
    void sendAlive();

signals:
    void blockReady();
};

#endif // ARDUINOSERIAL_H
