#-------------------------------------------------
#
# Project created by QtCreator 2017-05-07T16:19:00
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = BuildMonitor
TEMPLATE = app
QMAKE_CXXFLAGS += -std=c++11

SOURCES += main.cpp\
    BuildMonitor.cpp \
    BuildMonitorServerCommunication.cpp \
    JenkinsCommunication.cpp \
    ServerOverviewTable.cpp \
    Settings.cpp \
    SettingsDialog.cpp

HEADERS  += \
    BuildMonitor.h \
    BuildMonitorServerCommunication.h \
    FixInformation.h \
    JenkinsCommunication.h \
    ProjectInformation.h \
    ProjectStatus.h \
    ServerOverviewTable.h \
    Settings.h \
    SettingsDialog.h \
    SingleInstanceMode.h

FORMS    += buildmonitor.ui \
    BuildMonitor.ui \
    Settings.ui

RESOURCES += \
    BuildMonitor.qrc

DISTFILES +=
