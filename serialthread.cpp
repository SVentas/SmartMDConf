#include "serialthread.h"

#include <QtSerialPort/QSerialPort>
#include <QDebug>

#define SERIAL_WRITE_TIMEOUT_MS         20
#define SERIAL_READ_TIMEOUT_MS          20
#define SERIAL_READ_TIMEOUT_EXTRA_MS    10
#define SERIAL_DISCONNECT_TIMEOUT_MS    5000

QT_USE_NAMESPACE

/**
 * @brief SerialThread::SerialThread
 * @param parent
 */
SerialThread::SerialThread(QObject *parent) :
    QThread(parent),
    m_quit(false)
{
    // Empty;
}

/**
 * @brief SerialThread::~SerialThread
 */
SerialThread::~SerialThread()
{
    if (isRunning()) {
        disconnect();
    }
}

/**
 * @brief SerialThread::connect
 * @param portName
 */
void SerialThread::connect(const QString &portName)
{
    m_mutex.lock();
    m_portName = portName;
    m_quit = false;
    m_txBuf.clear();
    m_rxBuf.clear();
    m_mutex.unlock();

    if (!isRunning()) {
        start();
    }
}

/**
 * @brief SerialThread::disconnect
 */
void SerialThread::disconnect()
{
    m_mutex.lock();
    m_quit = true;
    m_mutex.unlock();

    if (!wait(SERIAL_DISCONNECT_TIMEOUT_MS)) {
        qDebug() << "Failed to terminate serial thread!";
    }
}

/**
 * @brief SerialThread::run
 */
void SerialThread::run()
{
    QSerialPort serial;

    serial.setPortName(m_portName);
    serial.setBaudRate(115200);
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);
    if (!serial.open(QIODevice::ReadWrite)) {
        qDebug() << "Connection failed!";
        emit this->serialError(tr("Can't open %1, error code %2. %3.")
            .arg(m_portName).arg(serial.error()).arg(serial.errorString()));
        return;
    }

    qDebug() << "Serial Thread is ready...";

    /* Clear buffer. */
    (void)serial.readAll();

    while (!m_quit) {
        /* Protect shared resources while thread is working. */
        m_mutex.lock();

        if (m_txBuf.size() > 0) {
            qint64 bytesWritten = serial.write(m_txBuf);
            if (serial.waitForBytesWritten(SERIAL_WRITE_TIMEOUT_MS)) {
                m_txBuf.remove(0, bytesWritten);
            } else {
                qDebug() << "Write request timeout!";
                /* Unlock resources and exit. */
                m_mutex.unlock();
                emit serialTimeout(tr("Write request timeout!"));
                break;
            }
        }

        /* Unlock resources. */
        m_mutex.unlock();

        if (serial.waitForReadyRead(SERIAL_READ_TIMEOUT_MS)) {
            m_rxBuf += serial.readAll();
            while (serial.waitForReadyRead(SERIAL_READ_TIMEOUT_EXTRA_MS)) {
                m_rxBuf += serial.readAll();
            }

            while (getMessage()) {
                processMessage();
            }
        }
    }

    qDebug() << "Serial Thread is terminating...";
    serial.close();
}

/**
 * @brief SerialThread::write
 * @param ba
 */
void SerialThread::write(const QByteArray &ba)
{
    m_mutex.lock();
    m_txBuf.append(ba);
    m_mutex.unlock();
}

/**
 * @brief SerialThread::getMessage
 * @return
 */
bool SerialThread::getMessage()
{
    static bool fContinueS = false;
    static quint8 waitCntS = 0;

    if (fContinueS) {
        if (m_rxBuf.size() >= m_msg.data_size) {
            fContinueS = false;
            return true;
        } else if (++waitCntS > 2) {
            fContinueS = false;
            /* Message is still not complete. Something wrong with communication?!.
             * Drop the message, clear the input buffer and start all over again.
             */
            m_rxBuf.clear();
            qDebug() << "Message still not comlete!";
        }
    } else if (m_rxBuf.size() >= TELEMETRY_MSG_HDR_SIZE) {
        /* Get new message header. */
        memcpy((void *)&m_msg, (const void*)m_rxBuf.constData(), TELEMETRY_MSG_HDR_SIZE);
        m_rxBuf.remove(0, TELEMETRY_MSG_HDR_SIZE);
        /* Check if message header is not corrupted. */
        if ((m_msg.signature == TELEMETRY_MSG_SIGNATURE) &&
            (m_msg.data_size <= TELEMETRY_MSG_SIZE_BYTES_MAX)) {
            if (m_rxBuf.size() >= m_msg.data_size) {
                /* Whole message is in the buffer. */
                return true;
            } else {
                /* Message is not complete. Wait for another iteration. */
                fContinueS = true;
                waitCntS = 0;
            }
        } else {
            /* Corrupted header received. Clear input buffer. */
            m_rxBuf.clear();
            qDebug() << "Message header corrupted!";
        }
    }

    return false;
}

/**
 * @brief SerialThread::processMessage
 */
void SerialThread::processMessage()
{
    qint16 *pBuf16;
    int numPts;
    static QVector<double> y(PLOTTING_BUF_DEPTH);
    static qint32 avgAccum = 0;
    static int avgCnt = 0;
    static int bufCnt = 0;
    bool fBufReady = false;

    switch (m_msg.msg_id) {
    case '.':
    case 'a':
    case 'b':
    case 'c':
    case 'o':
    case 'p':
        if (m_msg.data_size) {
            memcpy((void *)&(m_msg.data), (const void*)m_rxBuf.constData(), m_msg.data_size);
            m_rxBuf.remove(0, m_msg.data_size);
        }
        emit this->serialDataReady(m_msg);
        break;
    case 'r':
    case 's':
        pBuf16 = (qint16 *)m_rxBuf.constData();
        numPts = m_msg.data_size / 2;
        for (int i = 0; i < numPts; i++) {
            avgAccum += pBuf16[i];
            avgCnt++;
            avgCnt %= AVG_COUNTER_MAX;
            if (avgCnt == 0) {
                y[bufCnt++] = avgAccum / AVG_COUNTER_MAX;
                avgAccum = 0;
                bufCnt %= PLOTTING_BUF_DEPTH;
                if (bufCnt == 0) {
                    fBufReady = true;
                }
            }
        }
        m_rxBuf.remove(0, m_msg.data_size);
        if (fBufReady) {
            if (bufCnt) {
                qDebug() << "Buffer size mismatch:" << bufCnt;
            }
            emit this->streamDataReady(y);
        }
        break;
    default:
        m_rxBuf.remove(0, m_msg.data_size);
        qDebug() << "Unknown message received!";
        break;
    }
}
