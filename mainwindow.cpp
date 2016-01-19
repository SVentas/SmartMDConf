#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtSerialPort/QSerialPortInfo>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_serialPortList(new QComboBox),
    m_serialConnected(false),
    m_breakLoopFOC(false),
    m_breakLoopRAD(false),
    m_breakLoopFBK(false),
    m_breakLoopMotorSpeed(false)
{
    ui->setupUi(this);

    m_serialPortList->setMinimumWidth(250);
    ui->mainToolBar->insertWidget(ui->actionConnect, m_serialPortList);

    fillSerialPortInfo();

    connect(ui->actionConnect, SIGNAL(triggered()),
            this, SLOT(serialPortConnect()));
    connect(ui->actionStream, SIGNAL(triggered()),
            this, SLOT(streamingGO()));
    connect(ui->actionScan, SIGNAL(triggered()),
            this, SLOT(scaningGO()));
    connect(ui->actionReboot, SIGNAL(triggered()),
            this, SLOT(boardReboot()));

    connect(&m_serialTimer, SIGNAL(timeout()),
            this, SLOT(processTimeout()));

    connect(&m_serialThread, SIGNAL(serialError(QString)),
            this, SLOT(serialPortError(QString)), Qt::QueuedConnection);
    connect(&m_serialThread, SIGNAL(serialTimeout(QString)),
            this, SLOT(serialPortTimeout(QString)), Qt::QueuedConnection);
    connect(&m_serialThread, SIGNAL(serialDataReady(TelemetryMessage)),
            this, SLOT(processTelemetryMessage(TelemetryMessage)), Qt::QueuedConnection);
    connect(&m_serialThread, SIGNAL(streamDataReady(QVector<double>)),
            this, SLOT(processStreamData(QVector<double>)), Qt::QueuedConnection);

    connect(ui->sliderFOC, SIGNAL(valueChanged(int)),
            this, SLOT(actFOCUpdatePos(int)));
    connect(ui->sliderRAD, SIGNAL(valueChanged(int)),
            this, SLOT(actRADUpdatePos(int)));
    connect(ui->sliderFBK, SIGNAL(valueChanged(int)),
            this, SLOT(actFBKUpdatePos(int)));

    connect(ui->radioFE, SIGNAL(toggled(bool)),
            this, SLOT(streamingUpdateChannelID(bool)));
    connect(ui->radioCE, SIGNAL(toggled(bool)),
            this, SLOT(streamingUpdateChannelID(bool)));
    connect(ui->radioSum, SIGNAL(toggled(bool)),
            this, SLOT(streamingUpdateChannelID(bool)));
    connect(ui->radioChnA, SIGNAL(toggled(bool)),
            this, SLOT(streamingUpdateChannelID(bool)));
    connect(ui->radioChnB, SIGNAL(toggled(bool)),
            this, SLOT(streamingUpdateChannelID(bool)));
    connect(ui->radioChnC, SIGNAL(toggled(bool)),
            this, SLOT(streamingUpdateChannelID(bool)));
    connect(ui->radioChnD, SIGNAL(toggled(bool)),
            this, SLOT(streamingUpdateChannelID(bool)));

    connect(ui->sliderMotorSpeed, SIGNAL(valueChanged(int)),
            this, SLOT(motorSpeedUpdate(int)));
    connect(ui->pushMotorRead, SIGNAL(pressed()),
            this, SLOT(motorSettingsRead()));
    connect(ui->pushMotorUpdate, SIGNAL(pressed()),
            this, SLOT(motorSettingsUpdate()));

    ui->plotSlow->addGraph();
    /* line color red for first graph. */
    ui->plotSlow->graph(0)->setPen(QPen(Qt::red));
    ui->plotSlow->xAxis->setTickLabelType(QCPAxis::ltNumber);
    ui->plotSlow->xAxis->setAutoTickStep(false);
    ui->plotSlow->xAxis->setTickStep(512);

    ui->plotFast->addGraph();
    /* line color red for first graph. */
    ui->plotFast->graph(0)->setPen(QPen(Qt::red));
    ui->plotFast->xAxis->setTickLabelType(QCPAxis::ltNumber);
    ui->plotFast->xAxis->setAutoTickStep(false);
    ui->plotFast->xAxis->setTickStep(512);

    m_msg.msg_id    = TELEMETRY_MSG_NOMSG;
    m_msg.signature = TELEMETRY_MSG_SIGNATURE;
    m_msg.data_size = 0;

    m_pwmOutput.power = 1;
    m_pwmOutput.flags = 0;
}

MainWindow::~MainWindow()
{
    if (m_serialConnected) {
        if (m_serialTimer.isActive()) {
            m_serialTimer.stop();
            ui->actionStream->setText(tr("Stream"));
            ui->actionScan->setText(tr("Scan"));
        }
        m_serialThread.disconnect();
        ui->actionConnect->setText(tr("Connect"));
        m_serialPortList->setEnabled(true);
        ui->statusBar->showMessage(tr("Disconnected from: %1").arg(m_serialPortList->currentText()));
        ui->actionStream->setEnabled(false);
        ui->actionScan->setEnabled(false);
        m_serialConnected = false;
    }

    delete ui;
}

/**
 * @brief MainWindow::serialPortInfoFill
 */
void MainWindow::fillSerialPortInfo()
{
    m_serialPortList->clear();
    if (QSerialPortInfo::availablePorts().count() == 0) {
        m_serialPortList->addItem(tr("Serial port not detected"), "None");
        m_serialPortList->setEnabled(false);
        ui->actionConnect->setEnabled(false);
    } else {
        foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
            m_serialPortList->addItem(info.description() + ' ' + '(' + info.portName() + ')', info.portName());
        }
        m_serialPortList->setEnabled(true);
        ui->actionConnect->setEnabled(true);
    }
}

/**
 * @brief MainWindow::serialPortConnect
 */
void MainWindow::serialPortConnect()
{
    if (m_serialConnected) {
        if (m_serialTimer.isActive()) {
            m_serialTimer.stop();
            ui->actionStream->setText(tr("Stream"));
            ui->actionScan->setText(tr("Scan"));
        }
        m_serialThread.disconnect();
        ui->actionConnect->setText(tr("Connect"));
        m_serialPortList->setEnabled(true);
        ui->statusBar->showMessage(tr("Disconnected from: %1").arg(m_serialPortList->currentText()));
        ui->actionStream->setEnabled(false);
        ui->actionScan->setEnabled(false);
        m_serialConnected = false;
    } else {
        m_serialThread.connect(m_serialPortList->currentData().toString());
        ui->actionConnect->setText(tr("Disconnect"));
        m_serialPortList->setEnabled(false);
        ui->statusBar->showMessage(tr("Connected to: %1").arg(m_serialPortList->currentText()));
        m_serialConnected = true;
        ui->actionStream->setEnabled(true);
        ui->actionScan->setEnabled(true);
        boardReadSettings();
    }
}

/**
 * @brief MainWindow::streamingGO
 */
void MainWindow::streamingGO()
{
    if (!m_serialConnected) {
        return;
    }

    if (m_serialTimer.isActive()) {
        m_serialTimer.stop();
        ui->actionStream->setText(tr("Stream"));
        ui->actionScan->setEnabled(true);
    } else {
        m_msg.msg_id    = 'T';
        m_msg.signature = TELEMETRY_MSG_SIGNATURE;
        m_msg.data_size = sizeof(quint8);
        m_msg.data[0]   = 0;
        sendTelemetryMessage(m_msg);

        ui->actionScan->setEnabled(false);
        ui->actionStream->setText(tr("STOP"));
        m_serialTimer.start(20);
    }
}

/**
 * @brief MainWindow::scaningGO
 */
void MainWindow::scaningGO()
{
    if (!m_serialConnected) {
        return;
    }

    if (m_serialTimer.isActive()) {
        m_serialTimer.stop();
        ui->actionScan->setText(tr("Scan"));
        ui->actionStream->setEnabled(true);
    } else {
        m_msg.msg_id    = 'T';
        m_msg.signature = TELEMETRY_MSG_SIGNATURE;
        m_msg.data_size = sizeof(quint8);
        m_msg.data[0]   = 1;
        sendTelemetryMessage(m_msg);

        ui->actionStream->setEnabled(false);
        ui->actionScan->setText(tr("STOP"));
        m_serialTimer.start(20);
    }
}

/**
 * @brief MainWindow::serialPortError
 * @param s - error string;
 */
void MainWindow::serialPortError(const QString &s)
{
    if (m_serialConnected) {
        if (m_serialTimer.isActive()) {
            m_serialTimer.stop();
            ui->actionStream->setText(tr("Stream"));
            ui->actionScan->setText(tr("Scan"));
        }
        ui->actionConnect->setText(tr("Connect"));
        m_serialPortList->setEnabled(true);
        ui->actionStream->setEnabled(false);
        ui->actionScan->setEnabled(false);
        m_serialConnected = false;
        ui->statusBar->showMessage(s);
    }
}

/**
 * @brief MainWindow::serialPortTimeout
 * @param s - error string;
 */
void MainWindow::serialPortTimeout(const QString &s)
{
    if (m_serialConnected) {
        if (m_serialTimer.isActive()) {
            m_serialTimer.stop();
            ui->actionStream->setText(tr("Stream"));
            ui->actionScan->setText(tr("Scan"));
        }
        ui->actionConnect->setText(tr("Connect"));
        m_serialPortList->setEnabled(true);
        ui->actionStream->setEnabled(false);
        ui->actionScan->setEnabled(false);
        m_serialConnected = false;
        ui->statusBar->showMessage(s);
    }
}

/**
 * @brief MainWindow::writeTelemetryMessage
 * @param msg - telemetry message to be send.
 */
void MainWindow::sendTelemetryMessage(const TelemetryMessage &msg)
{
    QByteArray data;
    /* Send a message if and only if the serial connection is established. */
    if (m_serialConnected) {
        data.append((char *)&msg, msg.data_size + TELEMETRY_MSG_HDR_SIZE);
        m_serialThread.write(data);
    } else {
        QMessageBox::information(this, tr("No connection!"), tr("Connect to the serial port first!"));
    }
}

/**
 * @brief MainWindow::processTelemetryMessage
 * @param msg - telemetry message to be processed.
 */
void MainWindow::processTelemetryMessage(const TelemetryMessage &msg)
{
    quint16 utmp16;
    quint32 utmp32;
    static quint32 newPeriodCnt = 0;

    switch (msg.msg_id) {
    /*
     * T R A N S M I T T E R   S E C T I O N
     */
    case '.':
        if (msg.data_size == 0) {
            newPeriodCnt++;
            qDebug() << newPeriodCnt << "New period detected.";
        }
        break;
    case 'A':
        break;

    /*
     * R E C E I V E R   S E C T I O N
     */
    case 'a': /* Get FOC actuator position. */
        if (msg.data_size == sizeof(quint16)) {
            utmp16 = ((quint16*)msg.data)[0];
            if (utmp16 != ui->sliderFOC->value()) {
                m_breakLoopFOC = true;
                ui->sliderFOC->setValue(utmp16);
            }
        }
        break;
    case 'b': /* Get RAD actuator position. */
        if (msg.data_size == sizeof(quint16)) {
            utmp16 = ((quint16*)msg.data)[0];
            if (utmp16 != ui->sliderRAD->value()) {
                m_breakLoopRAD = true;
                ui->sliderRAD->setValue(utmp16);
            }
        }
        break;
    case 'c': /* Get FBK actuator position. */
        if (msg.data_size == sizeof(quint16)) {
            utmp16 = ((quint16*)msg.data)[0];
            if (utmp16 != ui->sliderFBK->value()) {
                m_breakLoopFBK = true;
                ui->sliderFBK->setValue(utmp16);
            }
        }
        break;
    case 'o': /* Get motor settings. */
        if (msg.data_size == sizeof(m_pwmOutput)) {
            m_pwmOutput.power = msg.data[0];
            m_pwmOutput.flags = msg.data[1];
            motorSetSettings();
        }
        break;
    case 'p': /* Get motor speed. */
        if (msg.data_size == sizeof(utmp32)) {
            utmp32 = ((quint32*)msg.data)[0] / 64;
            if ((int)utmp32 != ui->sliderMotorSpeed->value()) {
                m_breakLoopMotorSpeed = true;
                ui->sliderMotorSpeed->setValue(utmp32);
            }
        }
        break;
    default:
        qDebug() << "Unhandled message received!";
    }
}

#define SAMPLES_PER_PLOT    2048

/**
 * @brief MainWindow::processStreamData
 * @param x
 * @param y
 */
void MainWindow::processStreamData(QVector<double> y)
{
    static qint64 sampleCntSlowS = 1;
    static qint64 sampleCntFastS = 1;
    static quint8 updateCntS = 1;

    QVector<double> x(PLOTTING_BUF_DEPTH);
    double accumY = 0.0;

    for (int i = 0; i < PLOTTING_BUF_DEPTH; i++) {
        x[i] = sampleCntFastS++;
        accumY += y[i];
    }

    accumY /= PLOTTING_BUF_DEPTH;
    ui->plotSlow->graph(0)->addData(sampleCntSlowS++, accumY);
    ui->plotFast->graph(0)->addData(x, y);

    if (sampleCntSlowS > SAMPLES_PER_PLOT) {
        ui->plotSlow->graph(0)->removeDataBefore(sampleCntSlowS - SAMPLES_PER_PLOT);
    }

    if (sampleCntFastS > SAMPLES_PER_PLOT) {
        ui->plotFast->graph(0)->removeDataBefore(sampleCntFastS - SAMPLES_PER_PLOT);
    }

    if (updateCntS++ > (16 - 1)) {
        ui->plotSlow->graph(0)->rescaleValueAxis();
        ui->plotSlow->xAxis->setRange(sampleCntSlowS, SAMPLES_PER_PLOT, Qt::AlignRight);
        ui->plotSlow->replot();

        ui->plotFast->graph(0)->rescaleValueAxis();
        ui->plotFast->xAxis->setRange(sampleCntFastS, SAMPLES_PER_PLOT, Qt::AlignRight);
        ui->plotFast->replot();

        updateCntS = 1;
    }
}

/**
 * @brief MainWindow::processTimeout
 */
void MainWindow::processTimeout()
{
    m_msg.msg_id    = 's';
    m_msg.signature = TELEMETRY_MSG_SIGNATURE;
    m_msg.data_size = 0;

    sendTelemetryMessage(m_msg);
}

/**
 * @brief actFOCUpdatePos
 * @param pos - new position of the FOC actuator.
 */
void MainWindow::actFOCUpdatePos(int pos)
{
    ui->labelFOCPos->setText(tr("FOC actuator position (%1):").arg(pos));

    m_msg.msg_id    = 'A';
    m_msg.signature = TELEMETRY_MSG_SIGNATURE;
    m_msg.data_size = sizeof(quint16);
    memcpy((void *)m_msg.data, (void *)&pos, m_msg.data_size);

    if (m_breakLoopFOC) {
        m_breakLoopFOC = false;
    } else {
        sendTelemetryMessage(m_msg);
    }
}

/**
 * @brief actRADUpdatePos
 * @param pos - new position of the RAD actuator.
 */
void MainWindow::actRADUpdatePos(int pos)
{
    ui->labelRADPos->setText(tr("RAD actuator position (%1):").arg(pos));
    m_msg.msg_id    = 'B';
    m_msg.signature = TELEMETRY_MSG_SIGNATURE;
    m_msg.data_size = sizeof(quint16);
    memcpy((void *)m_msg.data, (void *)&pos, m_msg.data_size);

    if (m_breakLoopRAD) {
        m_breakLoopRAD = false;
    } else {
        sendTelemetryMessage(m_msg);
    }
}

/**
 * @brief actFBKUpdatePos
 * @param pos - new position of the FBK actuator.
 */
void MainWindow::actFBKUpdatePos(int pos)
{
    ui->labelFBKPos->setText(tr("FBK actuator position (%1):").arg(pos));
    m_msg.msg_id    = 'C';
    m_msg.signature = TELEMETRY_MSG_SIGNATURE;
    m_msg.data_size = sizeof(quint16);
    memcpy((void *)m_msg.data, (void *)&pos, m_msg.data_size);

    if (m_breakLoopFBK) {
        m_breakLoopFBK = false;
    } else {
        sendTelemetryMessage(m_msg);
    }
}

/**
 * @brief MainWindow::motorSpeedUpdate
 * @param speed - new speed of the motor.
 */
void MainWindow::motorSpeedUpdate(int speed)
{
    qint32 newSpeed = speed * 64;
    ui->labelMotorSpeed->setText(tr("Motor Speed (%1):").arg(speed));
    m_msg.msg_id    = 'P';
    m_msg.signature = TELEMETRY_MSG_SIGNATURE;
    m_msg.data_size = sizeof(newSpeed);
    memcpy((void *)m_msg.data, (void *)&newSpeed, m_msg.data_size);

    if (m_breakLoopMotorSpeed) {
        m_breakLoopMotorSpeed = false;
    } else {
        sendTelemetryMessage(m_msg);
    }
}

void MainWindow::motorSettingsRead()
{
    m_msg.msg_id    = 'o';
    m_msg.signature = TELEMETRY_MSG_SIGNATURE;
    m_msg.data_size = 0;
    sendTelemetryMessage(m_msg);
}

void MainWindow::motorSettingsUpdate()
{
    motorGetSettings();

    m_msg.msg_id    = 'O';
    m_msg.signature = TELEMETRY_MSG_SIGNATURE;
    m_msg.data_size = sizeof(m_pwmOutput);
    memcpy((void *)m_msg.data, (void *)&m_pwmOutput, m_msg.data_size);
    sendTelemetryMessage(m_msg);
}

void MainWindow::motorGetSettings()
{
    m_pwmOutput.power = (quint8)ui->spinMotorPower->value();

    if (ui->checkMotorRev->isChecked()) {
        m_pwmOutput.flags |= PWM_OUT_FLAG_REVERSE;
    } else {
        m_pwmOutput.flags &= ~PWM_OUT_FLAG_REVERSE;
    }
    qDebug() << "Motor power:" << m_pwmOutput.power \
             << "flags:" << m_pwmOutput.flags;
}

void MainWindow::motorSetSettings()
{
    ui->spinMotorPower->setValue(m_pwmOutput.power);
    if (m_pwmOutput.flags & PWM_OUT_FLAG_REVERSE) {
        ui->checkMotorRev->setChecked(true);
    } else {
        ui->checkMotorRev->setChecked(false);
    }
}

/* Available streaming channels. */
#define STREAMING_CHANNEL_FE    0x00
#define STREAMING_CHANNEL_CE    0x01
#define STREAMING_CHANNEL_SUM   0x02
#define STREAMING_CHANNEL_A     0x03
#define STREAMING_CHANNEL_B     0x04
#define STREAMING_CHANNEL_C     0x05
#define STREAMING_CHANNEL_D     0x06

/**
 * @brief MainWindow::streamingUpdateChannelID
 */
void MainWindow::streamingUpdateChannelID(bool checked)
{
    if (!checked) {
        return;
    }

    m_msg.msg_id    = 'S';
    m_msg.signature = TELEMETRY_MSG_SIGNATURE;
    m_msg.data_size = sizeof(quint8);

    if (ui->radioCE->isChecked()) {
        m_msg.data[0] = STREAMING_CHANNEL_CE;
    } else if (ui->radioSum->isChecked()) {
        m_msg.data[0] = STREAMING_CHANNEL_SUM;
    } else if (ui->radioChnA->isChecked()) {
        m_msg.data[0] = STREAMING_CHANNEL_A;
    } else if (ui->radioChnB->isChecked()) {
        m_msg.data[0] = STREAMING_CHANNEL_B;
    } else if (ui->radioChnC->isChecked()) {
        m_msg.data[0] = STREAMING_CHANNEL_C;
    }  else if (ui->radioChnD->isChecked()) {
        m_msg.data[0] = STREAMING_CHANNEL_D;
    } else {
        m_msg.data[0] = STREAMING_CHANNEL_FE;
    }

    sendTelemetryMessage(m_msg);
}

/**
 * @brief MainWindow::boardReboot
 */
void MainWindow::boardReboot()
{
    m_msg.msg_id    = 'X';
    m_msg.signature = TELEMETRY_MSG_SIGNATURE;
    m_msg.data_size = 0;
    sendTelemetryMessage(m_msg);
}

void MainWindow::boardReadSettings()
{
    /* Reads FOC actuator position. */
    m_msg.msg_id    = 'a';
    m_msg.signature = TELEMETRY_MSG_SIGNATURE;
    m_msg.data_size = 0;
    sendTelemetryMessage(m_msg);

    /* Reads RAD actuator position. */
    m_msg.msg_id    = 'b';
    sendTelemetryMessage(m_msg);

    /* Reads FBK actuator position. */
    m_msg.msg_id    = 'c';
    sendTelemetryMessage(m_msg);

    /* Reads Motor speed. */
    m_msg.msg_id    = 'o';
    sendTelemetryMessage(m_msg);

    /* Reads Motor PWM settings. */
    m_msg.msg_id    = 'p';
    sendTelemetryMessage(m_msg);
}
