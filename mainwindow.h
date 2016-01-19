#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QTimer>

#include "telemetry.h"
#include "serialthread.h"

#define PWM_OUT_PITCH           0x00
#define PWM_OUT_ROLL            0x01

#define PWM_OUT_FLAG_REVERSE    0x01
#define PWM_OUT_FLAG_USE_THI    0x02
#define PWM_OUT_FLAG_DISABLED   0x04

typedef struct tagPWMOutputStruct {
  quint8 power;
  quint8 flags;
} __attribute__((packed)) PWMOutputStruct, *PPWMOutputStruct;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void serialPortConnect();
    void serialPortError(const QString &s);
    void serialPortTimeout(const QString &s);
    void streamingGO();
    void scaningGO();
    void streamingUpdateChannelID(bool checked);
    void actFOCUpdatePos(int pos);
    void actRADUpdatePos(int pos);
    void actFBKUpdatePos(int pos);
    void motorSpeedUpdate(int speed);
    void motorSettingsRead(void);
    void motorSettingsUpdate(void);
    void boardReboot();
    void processTelemetryMessage(const TelemetryMessage &msg);
    void processStreamData(QVector<double> y);
    void processTimeout();

private:
    void boardReadSettings();
    void motorGetSettings();
    void motorSetSettings();
    void fillSerialPortInfo();
    void sendTelemetryMessage(const TelemetryMessage &msg);

private:
    Ui::MainWindow *ui;
    QComboBox *m_serialPortList;
    SerialThread m_serialThread;
    QTimer m_serialTimer;
    bool m_serialConnected;
    TelemetryMessage m_msg;
    PWMOutputStruct m_pwmOutput;
    bool m_breakLoopFOC;
    bool m_breakLoopRAD;
    bool m_breakLoopFBK;
    bool m_breakLoopMotorSpeed;
};

#endif // MAINWINDOW_H
