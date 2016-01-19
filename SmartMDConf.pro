#-------------------------------------------------
#
# Project created by QtCreator 2015-10-29T22:16:02
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets serialport printsupport

TARGET = SmartMDConf
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp\
        serialthread.cpp\
        3rdparty/qcustomplot.cpp

HEADERS  += mainwindow.h\
        serialthread.h\
        telemetry.h\
        3rdparty/qcustomplot.h

FORMS    += mainwindow.ui
