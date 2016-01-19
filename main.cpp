#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qRegisterMetaType<TelemetryMessage>();
    qRegisterMetaType<QVector<double> >();
    MainWindow w;
    w.show();

    return a.exec();
}
