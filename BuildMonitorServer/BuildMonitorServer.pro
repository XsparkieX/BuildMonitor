#-------------------------------------------------
#
# Project created by QtCreator 2017-05-29T21:04:03
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = BuildMonitorServer
TEMPLATE = app

unix:QMAKE_CXXFLAGS += -std=c++11

SOURCES += main.cpp\
    AcceptThread.cpp \
    BuildMonitorServer.cpp \
    Server.cpp

HEADERS  += AcceptThread.h \
    BuildMonitorServer.h \
    FixInfo.h \
    Server.h

RESOURCES += \
    BuildMonitorServer.qrc

FORMS += \
    BuildMonitorServer.ui
