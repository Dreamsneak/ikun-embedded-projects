#-------------------------------------------------
#
# Project created by QtCreator 2026-05-16T00:16:20
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = iKun_Power_Analyze
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS
#修改iKun_USB_IO_DEBUG_Helper的图标
RC_ICONS = ikun_Power.ico

SOURCES += \
        main.cpp \
        mainwindow.cpp \
        chartwidget.cpp

HEADERS += \
        mainwindow.h \
        chartwidget.h

FORMS += \
        mainwindow.ui

RESOURCES += \
        resources.qrc
