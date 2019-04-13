#-------------------------------------------------
#
# Project created by QtCreator 2017-05-07T16:19:00
#
#-------------------------------------------------

QT       += core gui network
win32:QT += winextras

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = BuildMonitor
TEMPLATE = app
unix:QMAKE_CXXFLAGS += -std=c++11
unix:QMAKE_LFLAGS += -no-pie

SOURCES += main.cpp\
    BuildMonitor.cpp \
    BuildMonitorServerCommunication.cpp \
    BuildMonitorServerWorker.cpp \
    JenkinsCommunication.cpp \
    ProjectPickerDialog.cpp \
    ServerOverviewTable.cpp \
    Settings.cpp \
    SettingsDialog.cpp \
    TrayContextMenu.cpp

HEADERS  += \
    BuildMonitor.h \
    BuildMonitorServerCommunication.h \
    BuildMonitorServerWorker.h \
    FixInformation.h \
    JenkinsCommunication.h \
    ProjectPickerDialog.h \
    ProjectInformation.h \
    ProjectStatus.h \
    ServerOverviewTable.h \
    Settings.h \
    SettingsDialog.h \
    SingleInstanceMode.h \
    TrayContextAction.h \
    TrayContextMenu.h

FORMS    += BuildMonitor.ui \
	ProjectPicker.ui \
    Settings.ui

RESOURCES += \
    BuildMonitor.qrc

DISTFILES +=
