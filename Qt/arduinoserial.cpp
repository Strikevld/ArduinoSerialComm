#include "arduinoserial.h"
#include <QThread>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>

quint16 ArduinoSerial::getQuint16(QByteArray arr) {
    quint32 ret;
    char * chret = (char*)&ret;
    chret[0] = arr.data()[0];
    chret[1] = arr.data()[1];
    return ret;
}

QByteArray ArduinoSerial::getDataFromQuint16(quint16 val)
{
    QByteArray ret;
    char * chret = (char*)&val;
    ret.append(chret[0]);
    ret.append(chret[1]);
    return ret;
}

void ArduinoSerial::sleep(int msecs)
{
    QEventLoop loop;
    QTimer::singleShot(msecs, &loop, SLOT(quit()));
    loop.exec();
}

ArduinoSerial::ArduinoSerial(QObject *parent) : QSerialPort(parent), aliveTimer(this)
{
    connect(&aliveTimer,&QTimer::timeout,this,&ArduinoSerial::sendAlive);
    aliveTimer.setInterval(5000);
    setTimeout();
}

ArduinoSerial::ArduinoSerial(const QSerialPortInfo &serialPortInfo, QObject *parent) : QSerialPort(serialPortInfo, parent), aliveTimer(this)
{
    connect(&aliveTimer,&QTimer::timeout,this,&ArduinoSerial::sendAlive);
    aliveTimer.setInterval(5000);
    setTimeout();
}

ArduinoSerial::ArduinoSerial(const QString &name, QObject *parent) : QSerialPort(name, parent), aliveTimer(this)
{
    connect(&aliveTimer,&QTimer::timeout,this,&ArduinoSerial::sendAlive);
    aliveTimer.setInterval(5000);
    setTimeout();
}

ArduinoSerial::~ArduinoSerial()
{
    if (isOpen()) {
        close();
    }
}

void ArduinoSerial::openStandart(QSerialPort::BaudRate baud_Rate, QSerialPort::DataBits dataBits,
                                 QSerialPort::Parity aParity, QSerialPort::StopBits stopBits,
                                 QSerialPort::FlowControl flow_Control, int onErrorRetryCount)
{
    sizeReaded = false;
    inputOk = false;
    writing = false;
    initOk = false;
    initTimeout = false;
    initMaxSize = false;
    nextBlockSize = 0;
    chunkSize = 0;
    retryCount = onErrorRetryCount;
    blocks.clear();
    aliveTimer.stop();
    replyOk[0] = 1; replyOk[1] = '\0';
    aliveData[0] = 3; aliveData[1] = '\0';
    if (isOpen()) {
        close();
    }
    connect(this, &ArduinoSerial::readyRead, this, &ArduinoSerial::onReadyRead);
    setBaudRate(baud_Rate);
    setDataBits(dataBits);
    setParity(aParity);
    setStopBits(stopBits);
    setFlowControl(flow_Control);
    zero[0] = 0; zero[1] = '\0';
    for (int i = 0; i < retryCount; i++) {
        if (!open(QSerialPort::ReadWrite)) {
            sleep(2000);
            continue;
        }
        sleep(50);

        char two[2]; two[0] = 2; two[1] = '\0';

        write(two, 1);
        for (int x = 0; x < retryCount; x++) {
            sleep(50);
            if (initTimeout) {
                break;
            }
        }
        if (!initTimeout) {
            close();
            sleep(3000);
            continue;
        }

        write(replyOk, 1);
        for (int x = 0; x < retryCount; x++) {
            sleep(50);
            if (initMaxSize) {
                break;
            }
        }
        if (!initMaxSize) {
            initTimeout = false;
            close();
            sleep(3000);
            continue;
        }

        write(zero, 1);
        for (int x = 0; x < retryCount; x++) {
            sleep(50);
            if (initOk) {
                aliveTimer.setInterval(static_cast<int>(timeout));
                aliveTimer.start();
                return;
            }
        }

        initTimeout = false;
        initMaxSize = false;
        close();
        sleep(3000);
    }
}

void ArduinoSerial::setTimeout(qint64 timeMS)
{
    timeout = timeMS;
}

qint64 ArduinoSerial::write(const char *data, qint64 maxSize)
{
    bool timerAct = aliveTimer.isActive();
    aliveTimer.stop();
    qint64 ret = QSerialPort::write(data, maxSize);
    if (timerAct&&isOpen()) {
        aliveTimer.start();
    }
    return ret;
}

qint64 ArduinoSerial::write(const QByteArray &byteArray)
{
    bool timerAct = aliveTimer.isActive();
    aliveTimer.stop();
    QByteArray data;
    inputOk = false;
    writing = true;
    quint16 datasize = static_cast<quint16>(byteArray.size());
    data = getDataFromQuint16(datasize);
    QSerialPort::write(data);
    for (int i = 0; i < retryCount; i++) {
        sleep(50);
        if (inputOk)
            break;
    }
    if (!inputOk) {
        writing = false;

        if (timerAct&&isOpen()) {
            aliveTimer.start();
        }
        return 0;
    }

    int remainig = byteArray.size();
    int dpos = 0;
    int chSize = qMin(chunkSize, remainig);

    while (remainig) {
        inputOk = false;
        data = byteArray.mid(dpos, chSize);
        QSerialPort::write(data);
        for (int i = 0; i < retryCount; i++) {
            sleep(50);
            if (inputOk)
                break;
        }
        if (!inputOk) {
            writing = false;

            if (timerAct&&isOpen()) {
                aliveTimer.start();
            }
            return 0;
        }
        dpos += chSize;
        remainig -= chSize;
        chSize = qMin(chunkSize, remainig);
    }

    inputOk = false;
    writing = false;

    if (timerAct&&isOpen()) {
        aliveTimer.start();
    }
    return byteArray.size();
}

int ArduinoSerial::blockPos(const QByteArray &firstSymbolsOfWantedBlock)
{
    if (firstSymbolsOfWantedBlock.isEmpty())
        return -1;
    for (int i = 0; i < blocks.size(); i++) {
        if (blocks[i].size() >= firstSymbolsOfWantedBlock.size()) {
            if (blocks[i].left(firstSymbolsOfWantedBlock.size()) == firstSymbolsOfWantedBlock)
                return i;
        }
    }
    return -1;
}

void ArduinoSerial::onReadyRead()
{
    bool timerAct = aliveTimer.isActive();
    aliveTimer.stop();
    if (!initTimeout) {
        QByteArray data = readAll();
        quint16 ui16 = getQuint16(data);
        timeout = static_cast<qint64>(ui16);
        initTimeout = true;
        if (timerAct&&isOpen()) {
            aliveTimer.start();
        }
        return;
    }
    if (!initMaxSize) {
        QByteArray data = readAll();
        quint16 ui16 = getQuint16(data);
        maxBufferSize = static_cast<int>(ui16);
        initMaxSize = true;
        if (timerAct&&isOpen()) {
            aliveTimer.start();
        }
        return;
    }
    if (!initOk) {
        QByteArray data = readAll();
        chunkSize = (int)((quint8)data[0]);
        initOk = true;
        if (timerAct&&isOpen()) {
            aliveTimer.start();
        }
        return;
    }
    if (writing) {
        QByteArray data = readAll();
        if (data.isEmpty()) {
            data = readAll();
            if (data.isEmpty()) {
                if (timerAct&&isOpen()) {
                    aliveTimer.start();
                }
                return;
            }
        }
        char chr = data.at(0);
        if (chr == 1) {
            inputOk = true;
        }
        if (timerAct&&isOpen()) {
            aliveTimer.start();
        }
        return;
    }

    QElapsedTimer et; et.start();
    while (true) {
        if (!sizeReaded) {
            buffer.clear();
            et.restart();
            while (et.elapsed() < timeout) {
                if ((bytesAvailable() >= sizeof (quint16))) {
                    QByteArray data = read(sizeof (quint16));
                    quint16 ui16 = getQuint16(data);
                    nextBlockSize = static_cast<qint64>(ui16);
                    if(nextBlockSize > maxBufferSize) {
                        nextBlockSize = 0;
                        continue;
                    }
                    remainingRead = static_cast<int>(nextBlockSize);
                    sizeReaded = true;
                    break;
                }
                sleep(50);
            }
            if (sizeReaded) {
                write(replyOk, 1);
            }
        }
        if (!bytesAvailable()) {
            aliveTimer.stop();
            if (timerAct&&isOpen()) {
                aliveTimer.start();
            }
            return;
        }
        if (!sizeReaded) {
            if (timerAct&&isOpen()) {
                aliveTimer.start();
            }
            return;
        }

        et.restart();
        while (et.elapsed() < timeout) {
            int readSize = qMin(remainingRead, chunkSize);
            if ((bytesAvailable() >= readSize)) {
                QByteArray data = read(readSize);
                remainingRead -= readSize;
                buffer.append(data);
                write(replyOk, 1);
                if (buffer.size() >= nextBlockSize) {
                    blocks.enqueue(buffer);
                    if (!blockLeft.isEmpty()) {
                        if (buffer.size() >= blockLeft.size()) {
                            if (buffer.left(blockLeft.size()) == blockLeft)
                                break;
                        }
                    }
                    emit blockReady();
                    break;
                }
                if (!bytesAvailable()) {
                    if (timerAct&&isOpen()) {
                        aliveTimer.start();
                    }
                    return;
                }
            }
        }
        nextBlockSize = 0;
        remainingRead = 0;
        sizeReaded = false;
        if (bytesAvailable() < sizeof (quint16)) {
            if (timerAct&&isOpen()) {
                aliveTimer.start();
            }
            return;
        }
    }
}

void ArduinoSerial::sendAlive()
{
    if (!initOk) {
        aliveTimer.stop();
        return;
    }

    write(aliveData, 1);
}

bool ArduinoSerial::isBlockReady(const QByteArray &firstSymbolsOfWantedBlock)
{
    if (firstSymbolsOfWantedBlock.isEmpty())
        return !blocks.isEmpty();

    return (blockPos(firstSymbolsOfWantedBlock) != -1);
}

QByteArray ArduinoSerial::nextBlock(bool alwaysTimeout, const QByteArray &firstSymbolsOfWantedBlock)
{
    if (isBlockReady(firstSymbolsOfWantedBlock)) {
        if (firstSymbolsOfWantedBlock.isEmpty())
            return blocks.dequeue();
        int bpos = blockPos(firstSymbolsOfWantedBlock);
        if (bpos < 0)
            return QByteArray();
        return blocks.takeAt(bpos);
    }
    if (!alwaysTimeout)
        return QByteArray();
    blockLeft = firstSymbolsOfWantedBlock;
    QElapsedTimer et; et.start();
    while (et.elapsed() < timeout) {
        if (isBlockReady(firstSymbolsOfWantedBlock)) {
            blockLeft.clear();
            if (firstSymbolsOfWantedBlock.isEmpty()) {
                return blocks.dequeue();
            }
            int bpos = blockPos(firstSymbolsOfWantedBlock);
            if (bpos < 0) {
                return QByteArray();
            }
            return blocks.takeAt(bpos);
        }
        sleep(50);
    }
    blockLeft.clear();
    return QByteArray();
}
