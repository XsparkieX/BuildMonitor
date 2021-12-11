#-------------------------------------------------
#
# Project created by QtCreator 2017-05-07T16:19:00
#
#-------------------------------------------------

QT       += core gui
win32:QT += winextras

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = BuildMonitor
TEMPLATE = app
QMAKE_CXXFLAGS += -std=c++17
unix:QMAKE_LFLAGS += -no-pie

SOURCES += main.cpp\
    BuildMonitor.cpp \
    ServerOverviewTable.cpp \
    ServerOverviewTableEntry.cpp \
    Settings.cpp \
    SettingsDialog.cpp \
    TrayContextMenu.cpp \
	Utils.cpp

HEADERS  += \
    BuildMonitor.h \
    ServerOverviewTable.h \
    ServerOverviewTableEntry.h \
    Settings.h \
    SettingsDialog.h \
    SingleInstanceMode.h \
    TrayContextAction.h \
    TrayContextMenu.h \
	Utils.h

FORMS    += BuildMonitor.ui \
    Settings.ui

RESOURCES += \
    BuildMonitor.qrc

RC_FILE = BuildMonitor.rc

INCLUDEPATH += \
	"$$_PRO_FILE_PWD_/../build_monitor/capi/include"

win32:LIBS += \
	-L"$$_PRO_FILE_PWD_\\..\\build_monitor\\capi\\target\\release" -lbuild_monitor_capi.dll

unix:LIBS += \
    -L"$$_PRO_FILE_PWD_/../build_monitor/capi/target/release" -lbuild_monitor_capi

BUILD_MONITOR_DLL_SRC_PATH = $$shell_path($$clean_path("$${_PRO_FILE_PWD_}\\..\\build_monitor\\capi\\target\\release\\build_monitor_capi.dll"))
BUILD_MONITOR_DLL_DEST_PATH = $$shell_path($$clean_path("$${DESTDIR}"))

BuildMonitorCopy.commands = $$quote(cmd /c xcopy /Y $${BUILD_MONITOR_DLL_SRC_PATH} $${BUILD_MONITOR_DLL_DEST_PATH})

win32:QMAKE_EXTRA_TARGETS += BuildMonitorCopy
win32:POST_TARGETDEPS += BuildMonitorCopy

