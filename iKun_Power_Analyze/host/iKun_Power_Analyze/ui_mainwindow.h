/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.14.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *actionSave_Data;
    QAction *actionExit;
    QAction *actionPort_Settings;
    QAction *actionAbout;
    QWidget *centralWidget;
    QHBoxLayout *horizontalLayout_5;
    QVBoxLayout *leftLayout;
    QGroupBox *controlGroupBox;
    QHBoxLayout *controlLayout;
    QPushButton *showButton;
    QPushButton *clearButton;
    QPushButton *saveButton;
    QGroupBox *dataGroupBox;
    QGridLayout *dataGridLayout;
    QVBoxLayout *currentLayout;
    QLabel *currentTitleLabel;
    QLabel *currentValueLabel;
    QLabel *currentUnitLabel;
    QVBoxLayout *voltageLayout;
    QLabel *voltageTitleLabel;
    QLabel *voltageValueLabel;
    QLabel *voltageUnitLabel;
    QVBoxLayout *powerLayout;
    QLabel *powerTitleLabel;
    QLabel *powerValueLabel;
    QLabel *powerUnitLabel;
    QVBoxLayout *energyLayout;
    QLabel *energyTitleLabel;
    QLabel *energyValueLabel;
    QLabel *energyUnitLabel;
    QGroupBox *minMaxGroupBox;
    QGridLayout *minMaxGridLayout;
    QLabel *minCurrentLabel;
    QLabel *maxCurrentLabel;
    QLabel *minVoltageLabel;
    QLabel *maxVoltageLabel;
    QLabel *minPowerLabel;
    QLabel *maxPowerLabel;
    QLabel *minCurrentTitle;
    QLabel *maxCurrentTitle;
    QLabel *minVoltageTitle;
    QLabel *maxVoltageTitle;
    QLabel *minPowerTitle;
    QLabel *maxPowerTitle;
    QGroupBox *logGroupBox;
    QVBoxLayout *logLayout;
    QTextEdit *logTextEdit;
    QGroupBox *connectionStatusGroupBox;
    QHBoxLayout *connectionStatusLayout;
    QSpacerItem *horizontalSpacer;
    QLabel *connectionStatusLabel;
    QLabel *connectionStatusBall;
    QSpacerItem *horizontalSpacer_2;
    QVBoxLayout *rightLayout;
    QGroupBox *currentChartGroupBox;
    QVBoxLayout *currentChartLayout;
    QGroupBox *voltageChartGroupBox;
    QVBoxLayout *voltageChartLayout;
    QGroupBox *powerChartGroupBox;
    QVBoxLayout *powerChartLayout;
    QMenuBar *menuBar;
    QMenu *menuFile;
    QMenu *menuSettings;
    QMenu *menuHelp;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(1200, 800);
        actionSave_Data = new QAction(MainWindow);
        actionSave_Data->setObjectName(QString::fromUtf8("actionSave_Data"));
        actionExit = new QAction(MainWindow);
        actionExit->setObjectName(QString::fromUtf8("actionExit"));
        actionPort_Settings = new QAction(MainWindow);
        actionPort_Settings->setObjectName(QString::fromUtf8("actionPort_Settings"));
        actionAbout = new QAction(MainWindow);
        actionAbout->setObjectName(QString::fromUtf8("actionAbout"));
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        horizontalLayout_5 = new QHBoxLayout(centralWidget);
        horizontalLayout_5->setSpacing(6);
        horizontalLayout_5->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_5->setObjectName(QString::fromUtf8("horizontalLayout_5"));
        leftLayout = new QVBoxLayout();
        leftLayout->setSpacing(6);
        leftLayout->setObjectName(QString::fromUtf8("leftLayout"));
        controlGroupBox = new QGroupBox(centralWidget);
        controlGroupBox->setObjectName(QString::fromUtf8("controlGroupBox"));
        controlLayout = new QHBoxLayout(controlGroupBox);
        controlLayout->setSpacing(6);
        controlLayout->setContentsMargins(11, 11, 11, 11);
        controlLayout->setObjectName(QString::fromUtf8("controlLayout"));
        showButton = new QPushButton(controlGroupBox);
        showButton->setObjectName(QString::fromUtf8("showButton"));

        controlLayout->addWidget(showButton);

        clearButton = new QPushButton(controlGroupBox);
        clearButton->setObjectName(QString::fromUtf8("clearButton"));

        controlLayout->addWidget(clearButton);

        saveButton = new QPushButton(controlGroupBox);
        saveButton->setObjectName(QString::fromUtf8("saveButton"));

        controlLayout->addWidget(saveButton);


        leftLayout->addWidget(controlGroupBox);

        dataGroupBox = new QGroupBox(centralWidget);
        dataGroupBox->setObjectName(QString::fromUtf8("dataGroupBox"));
        dataGridLayout = new QGridLayout(dataGroupBox);
        dataGridLayout->setSpacing(6);
        dataGridLayout->setContentsMargins(11, 11, 11, 11);
        dataGridLayout->setObjectName(QString::fromUtf8("dataGridLayout"));
        currentLayout = new QVBoxLayout();
        currentLayout->setSpacing(6);
        currentLayout->setObjectName(QString::fromUtf8("currentLayout"));
        currentTitleLabel = new QLabel(dataGroupBox);
        currentTitleLabel->setObjectName(QString::fromUtf8("currentTitleLabel"));
        currentTitleLabel->setAlignment(Qt::AlignCenter);

        currentLayout->addWidget(currentTitleLabel);

        currentValueLabel = new QLabel(dataGroupBox);
        currentValueLabel->setObjectName(QString::fromUtf8("currentValueLabel"));
        currentValueLabel->setAlignment(Qt::AlignCenter);
        currentValueLabel->setStyleSheet(QString::fromUtf8("font-size: 32px; font-weight: bold; color: #00bcd4;"));

        currentLayout->addWidget(currentValueLabel);

        currentUnitLabel = new QLabel(dataGroupBox);
        currentUnitLabel->setObjectName(QString::fromUtf8("currentUnitLabel"));
        currentUnitLabel->setAlignment(Qt::AlignCenter);
        currentUnitLabel->setStyleSheet(QString::fromUtf8("font-size: 16px; color: #7f8c8d;"));

        currentLayout->addWidget(currentUnitLabel);


        dataGridLayout->addLayout(currentLayout, 0, 0, 1, 1);

        voltageLayout = new QVBoxLayout();
        voltageLayout->setSpacing(6);
        voltageLayout->setObjectName(QString::fromUtf8("voltageLayout"));
        voltageTitleLabel = new QLabel(dataGroupBox);
        voltageTitleLabel->setObjectName(QString::fromUtf8("voltageTitleLabel"));
        voltageTitleLabel->setAlignment(Qt::AlignCenter);

        voltageLayout->addWidget(voltageTitleLabel);

        voltageValueLabel = new QLabel(dataGroupBox);
        voltageValueLabel->setObjectName(QString::fromUtf8("voltageValueLabel"));
        voltageValueLabel->setAlignment(Qt::AlignCenter);
        voltageValueLabel->setStyleSheet(QString::fromUtf8("font-size: 32px; font-weight: bold; color: #4caf50;"));

        voltageLayout->addWidget(voltageValueLabel);

        voltageUnitLabel = new QLabel(dataGroupBox);
        voltageUnitLabel->setObjectName(QString::fromUtf8("voltageUnitLabel"));
        voltageUnitLabel->setAlignment(Qt::AlignCenter);
        voltageUnitLabel->setStyleSheet(QString::fromUtf8("font-size: 16px; color: #7f8c8d;"));

        voltageLayout->addWidget(voltageUnitLabel);


        dataGridLayout->addLayout(voltageLayout, 0, 1, 1, 1);

        powerLayout = new QVBoxLayout();
        powerLayout->setSpacing(6);
        powerLayout->setObjectName(QString::fromUtf8("powerLayout"));
        powerTitleLabel = new QLabel(dataGroupBox);
        powerTitleLabel->setObjectName(QString::fromUtf8("powerTitleLabel"));
        powerTitleLabel->setAlignment(Qt::AlignCenter);

        powerLayout->addWidget(powerTitleLabel);

        powerValueLabel = new QLabel(dataGroupBox);
        powerValueLabel->setObjectName(QString::fromUtf8("powerValueLabel"));
        powerValueLabel->setAlignment(Qt::AlignCenter);
        powerValueLabel->setStyleSheet(QString::fromUtf8("font-size: 32px; font-weight: bold; color: #ff9800;"));

        powerLayout->addWidget(powerValueLabel);

        powerUnitLabel = new QLabel(dataGroupBox);
        powerUnitLabel->setObjectName(QString::fromUtf8("powerUnitLabel"));
        powerUnitLabel->setAlignment(Qt::AlignCenter);
        powerUnitLabel->setStyleSheet(QString::fromUtf8("font-size: 16px; color: #7f8c8d;"));

        powerLayout->addWidget(powerUnitLabel);


        dataGridLayout->addLayout(powerLayout, 0, 2, 1, 1);

        energyLayout = new QVBoxLayout();
        energyLayout->setSpacing(6);
        energyLayout->setObjectName(QString::fromUtf8("energyLayout"));
        energyTitleLabel = new QLabel(dataGroupBox);
        energyTitleLabel->setObjectName(QString::fromUtf8("energyTitleLabel"));
        energyTitleLabel->setAlignment(Qt::AlignCenter);

        energyLayout->addWidget(energyTitleLabel);

        energyValueLabel = new QLabel(dataGroupBox);
        energyValueLabel->setObjectName(QString::fromUtf8("energyValueLabel"));
        energyValueLabel->setAlignment(Qt::AlignCenter);
        energyValueLabel->setStyleSheet(QString::fromUtf8("font-size: 32px; font-weight: bold; color: #e91e63;"));

        energyLayout->addWidget(energyValueLabel);

        energyUnitLabel = new QLabel(dataGroupBox);
        energyUnitLabel->setObjectName(QString::fromUtf8("energyUnitLabel"));
        energyUnitLabel->setAlignment(Qt::AlignCenter);
        energyUnitLabel->setStyleSheet(QString::fromUtf8("font-size: 16px; color: #7f8c8d;"));

        energyLayout->addWidget(energyUnitLabel);


        dataGridLayout->addLayout(energyLayout, 0, 3, 1, 1);


        leftLayout->addWidget(dataGroupBox);

        minMaxGroupBox = new QGroupBox(centralWidget);
        minMaxGroupBox->setObjectName(QString::fromUtf8("minMaxGroupBox"));
        minMaxGridLayout = new QGridLayout(minMaxGroupBox);
        minMaxGridLayout->setSpacing(6);
        minMaxGridLayout->setContentsMargins(11, 11, 11, 11);
        minMaxGridLayout->setObjectName(QString::fromUtf8("minMaxGridLayout"));
        minCurrentLabel = new QLabel(minMaxGroupBox);
        minCurrentLabel->setObjectName(QString::fromUtf8("minCurrentLabel"));
        minCurrentLabel->setAlignment(Qt::AlignCenter);
        minCurrentLabel->setStyleSheet(QString::fromUtf8("font-size: 14px; color: #ecf0f1;"));

        minMaxGridLayout->addWidget(minCurrentLabel, 0, 0, 1, 1);

        maxCurrentLabel = new QLabel(minMaxGroupBox);
        maxCurrentLabel->setObjectName(QString::fromUtf8("maxCurrentLabel"));
        maxCurrentLabel->setAlignment(Qt::AlignCenter);
        maxCurrentLabel->setStyleSheet(QString::fromUtf8("font-size: 14px; color: #ecf0f1;"));

        minMaxGridLayout->addWidget(maxCurrentLabel, 0, 1, 1, 1);

        minVoltageLabel = new QLabel(minMaxGroupBox);
        minVoltageLabel->setObjectName(QString::fromUtf8("minVoltageLabel"));
        minVoltageLabel->setAlignment(Qt::AlignCenter);
        minVoltageLabel->setStyleSheet(QString::fromUtf8("font-size: 14px; color: #ecf0f1;"));

        minMaxGridLayout->addWidget(minVoltageLabel, 0, 2, 1, 1);

        maxVoltageLabel = new QLabel(minMaxGroupBox);
        maxVoltageLabel->setObjectName(QString::fromUtf8("maxVoltageLabel"));
        maxVoltageLabel->setAlignment(Qt::AlignCenter);
        maxVoltageLabel->setStyleSheet(QString::fromUtf8("font-size: 14px; color: #ecf0f1;"));

        minMaxGridLayout->addWidget(maxVoltageLabel, 0, 3, 1, 1);

        minPowerLabel = new QLabel(minMaxGroupBox);
        minPowerLabel->setObjectName(QString::fromUtf8("minPowerLabel"));
        minPowerLabel->setAlignment(Qt::AlignCenter);
        minPowerLabel->setStyleSheet(QString::fromUtf8("font-size: 14px; color: #ecf0f1;"));

        minMaxGridLayout->addWidget(minPowerLabel, 0, 4, 1, 1);

        maxPowerLabel = new QLabel(minMaxGroupBox);
        maxPowerLabel->setObjectName(QString::fromUtf8("maxPowerLabel"));
        maxPowerLabel->setAlignment(Qt::AlignCenter);
        maxPowerLabel->setStyleSheet(QString::fromUtf8("font-size: 14px; color: #ecf0f1;"));

        minMaxGridLayout->addWidget(maxPowerLabel, 0, 5, 1, 1);

        minCurrentTitle = new QLabel(minMaxGroupBox);
        minCurrentTitle->setObjectName(QString::fromUtf8("minCurrentTitle"));
        minCurrentTitle->setAlignment(Qt::AlignCenter);
        minCurrentTitle->setStyleSheet(QString::fromUtf8("font-size: 11px; color: #7f8c8d;"));

        minMaxGridLayout->addWidget(minCurrentTitle, 1, 0, 1, 1);

        maxCurrentTitle = new QLabel(minMaxGroupBox);
        maxCurrentTitle->setObjectName(QString::fromUtf8("maxCurrentTitle"));
        maxCurrentTitle->setAlignment(Qt::AlignCenter);
        maxCurrentTitle->setStyleSheet(QString::fromUtf8("font-size: 11px; color: #7f8c8d;"));

        minMaxGridLayout->addWidget(maxCurrentTitle, 1, 1, 1, 1);

        minVoltageTitle = new QLabel(minMaxGroupBox);
        minVoltageTitle->setObjectName(QString::fromUtf8("minVoltageTitle"));
        minVoltageTitle->setAlignment(Qt::AlignCenter);
        minVoltageTitle->setStyleSheet(QString::fromUtf8("font-size: 11px; color: #7f8c8d;"));

        minMaxGridLayout->addWidget(minVoltageTitle, 1, 2, 1, 1);

        maxVoltageTitle = new QLabel(minMaxGroupBox);
        maxVoltageTitle->setObjectName(QString::fromUtf8("maxVoltageTitle"));
        maxVoltageTitle->setAlignment(Qt::AlignCenter);
        maxVoltageTitle->setStyleSheet(QString::fromUtf8("font-size: 11px; color: #7f8c8d;"));

        minMaxGridLayout->addWidget(maxVoltageTitle, 1, 3, 1, 1);

        minPowerTitle = new QLabel(minMaxGroupBox);
        minPowerTitle->setObjectName(QString::fromUtf8("minPowerTitle"));
        minPowerTitle->setAlignment(Qt::AlignCenter);
        minPowerTitle->setStyleSheet(QString::fromUtf8("font-size: 11px; color: #7f8c8d;"));

        minMaxGridLayout->addWidget(minPowerTitle, 1, 4, 1, 1);

        maxPowerTitle = new QLabel(minMaxGroupBox);
        maxPowerTitle->setObjectName(QString::fromUtf8("maxPowerTitle"));
        maxPowerTitle->setAlignment(Qt::AlignCenter);
        maxPowerTitle->setStyleSheet(QString::fromUtf8("font-size: 11px; color: #7f8c8d;"));

        minMaxGridLayout->addWidget(maxPowerTitle, 1, 5, 1, 1);


        leftLayout->addWidget(minMaxGroupBox);

        logGroupBox = new QGroupBox(centralWidget);
        logGroupBox->setObjectName(QString::fromUtf8("logGroupBox"));
        logLayout = new QVBoxLayout(logGroupBox);
        logLayout->setSpacing(6);
        logLayout->setContentsMargins(11, 11, 11, 11);
        logLayout->setObjectName(QString::fromUtf8("logLayout"));
        logTextEdit = new QTextEdit(logGroupBox);
        logTextEdit->setObjectName(QString::fromUtf8("logTextEdit"));
        logTextEdit->setReadOnly(true);

        logLayout->addWidget(logTextEdit);


        leftLayout->addWidget(logGroupBox);

        connectionStatusGroupBox = new QGroupBox(centralWidget);
        connectionStatusGroupBox->setObjectName(QString::fromUtf8("connectionStatusGroupBox"));
        connectionStatusLayout = new QHBoxLayout(connectionStatusGroupBox);
        connectionStatusLayout->setSpacing(6);
        connectionStatusLayout->setContentsMargins(11, 11, 11, 11);
        connectionStatusLayout->setObjectName(QString::fromUtf8("connectionStatusLayout"));
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        connectionStatusLayout->addItem(horizontalSpacer);

        connectionStatusLabel = new QLabel(connectionStatusGroupBox);
        connectionStatusLabel->setObjectName(QString::fromUtf8("connectionStatusLabel"));
        connectionStatusLabel->setStyleSheet(QString::fromUtf8("color: #e74c3c; font-size: 14px; font-weight: bold;"));

        connectionStatusLayout->addWidget(connectionStatusLabel);

        connectionStatusBall = new QLabel(connectionStatusGroupBox);
        connectionStatusBall->setObjectName(QString::fromUtf8("connectionStatusBall"));
        connectionStatusBall->setMinimumSize(QSize(20, 20));
        connectionStatusBall->setMaximumSize(QSize(20, 20));
        connectionStatusBall->setStyleSheet(QString::fromUtf8("border-radius: 10px; background-color: #e74c3c;"));

        connectionStatusLayout->addWidget(connectionStatusBall);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        connectionStatusLayout->addItem(horizontalSpacer_2);


        leftLayout->addWidget(connectionStatusGroupBox);


        horizontalLayout_5->addLayout(leftLayout);

        rightLayout = new QVBoxLayout();
        rightLayout->setSpacing(6);
        rightLayout->setObjectName(QString::fromUtf8("rightLayout"));
        currentChartGroupBox = new QGroupBox(centralWidget);
        currentChartGroupBox->setObjectName(QString::fromUtf8("currentChartGroupBox"));
        currentChartLayout = new QVBoxLayout(currentChartGroupBox);
        currentChartLayout->setSpacing(6);
        currentChartLayout->setContentsMargins(11, 11, 11, 11);
        currentChartLayout->setObjectName(QString::fromUtf8("currentChartLayout"));

        rightLayout->addWidget(currentChartGroupBox);

        voltageChartGroupBox = new QGroupBox(centralWidget);
        voltageChartGroupBox->setObjectName(QString::fromUtf8("voltageChartGroupBox"));
        voltageChartLayout = new QVBoxLayout(voltageChartGroupBox);
        voltageChartLayout->setSpacing(6);
        voltageChartLayout->setContentsMargins(11, 11, 11, 11);
        voltageChartLayout->setObjectName(QString::fromUtf8("voltageChartLayout"));

        rightLayout->addWidget(voltageChartGroupBox);

        powerChartGroupBox = new QGroupBox(centralWidget);
        powerChartGroupBox->setObjectName(QString::fromUtf8("powerChartGroupBox"));
        powerChartLayout = new QVBoxLayout(powerChartGroupBox);
        powerChartLayout->setSpacing(6);
        powerChartLayout->setContentsMargins(11, 11, 11, 11);
        powerChartLayout->setObjectName(QString::fromUtf8("powerChartLayout"));

        rightLayout->addWidget(powerChartGroupBox);


        horizontalLayout_5->addLayout(rightLayout);

        MainWindow->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(MainWindow);
        menuBar->setObjectName(QString::fromUtf8("menuBar"));
        menuBar->setGeometry(QRect(0, 0, 1200, 21));
        menuFile = new QMenu(menuBar);
        menuFile->setObjectName(QString::fromUtf8("menuFile"));
        menuSettings = new QMenu(menuBar);
        menuSettings->setObjectName(QString::fromUtf8("menuSettings"));
        menuHelp = new QMenu(menuBar);
        menuHelp->setObjectName(QString::fromUtf8("menuHelp"));
        MainWindow->setMenuBar(menuBar);
        statusBar = new QStatusBar(MainWindow);
        statusBar->setObjectName(QString::fromUtf8("statusBar"));
        statusBar->setStyleSheet(QString::fromUtf8("background: #1a1a2e; color: #ecf0f1;"));
        MainWindow->setStatusBar(statusBar);

        menuBar->addAction(menuFile->menuAction());
        menuBar->addAction(menuSettings->menuAction());
        menuBar->addAction(menuHelp->menuAction());
        menuFile->addAction(actionSave_Data);
        menuFile->addSeparator();
        menuFile->addAction(actionExit);
        menuSettings->addAction(actionPort_Settings);
        menuHelp->addAction(actionAbout);

        retranslateUi(MainWindow);
        QObject::connect(actionSave_Data, SIGNAL(triggered()), MainWindow, SLOT(onSaveButtonClicked()));

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "iKun Power Analyzer", nullptr));
        actionSave_Data->setText(QCoreApplication::translate("MainWindow", "Save Data", nullptr));
        actionExit->setText(QCoreApplication::translate("MainWindow", "Exit", nullptr));
        actionPort_Settings->setText(QCoreApplication::translate("MainWindow", "Port Settings", nullptr));
        actionAbout->setText(QCoreApplication::translate("MainWindow", "About", nullptr));
        controlGroupBox->setTitle(QCoreApplication::translate("MainWindow", "Control", nullptr));
        showButton->setText(QCoreApplication::translate("MainWindow", "Run", nullptr));
        clearButton->setText(QCoreApplication::translate("MainWindow", "Clear Data", nullptr));
        saveButton->setText(QCoreApplication::translate("MainWindow", "Save Data", nullptr));
        dataGroupBox->setTitle(QCoreApplication::translate("MainWindow", "Real-time Measurements", nullptr));
        currentTitleLabel->setText(QCoreApplication::translate("MainWindow", "Current", nullptr));
        currentValueLabel->setText(QCoreApplication::translate("MainWindow", "0.00", nullptr));
        currentUnitLabel->setText(QCoreApplication::translate("MainWindow", "A", nullptr));
        voltageTitleLabel->setText(QCoreApplication::translate("MainWindow", "Voltage", nullptr));
        voltageValueLabel->setText(QCoreApplication::translate("MainWindow", "0.00", nullptr));
        voltageUnitLabel->setText(QCoreApplication::translate("MainWindow", "V", nullptr));
        powerTitleLabel->setText(QCoreApplication::translate("MainWindow", "Power", nullptr));
        powerValueLabel->setText(QCoreApplication::translate("MainWindow", "0.00", nullptr));
        powerUnitLabel->setText(QCoreApplication::translate("MainWindow", "W", nullptr));
        energyTitleLabel->setText(QCoreApplication::translate("MainWindow", "Energy", nullptr));
        energyValueLabel->setText(QCoreApplication::translate("MainWindow", "0.00", nullptr));
        energyUnitLabel->setText(QCoreApplication::translate("MainWindow", "kWh", nullptr));
        minMaxGroupBox->setTitle(QCoreApplication::translate("MainWindow", "Min/Max Values", nullptr));
        minCurrentLabel->setText(QCoreApplication::translate("MainWindow", "0.00", nullptr));
        maxCurrentLabel->setText(QCoreApplication::translate("MainWindow", "0.00", nullptr));
        minVoltageLabel->setText(QCoreApplication::translate("MainWindow", "0.00", nullptr));
        maxVoltageLabel->setText(QCoreApplication::translate("MainWindow", "0.00", nullptr));
        minPowerLabel->setText(QCoreApplication::translate("MainWindow", "0.00", nullptr));
        maxPowerLabel->setText(QCoreApplication::translate("MainWindow", "0.00", nullptr));
        minCurrentTitle->setText(QCoreApplication::translate("MainWindow", "I Min (A)", nullptr));
        maxCurrentTitle->setText(QCoreApplication::translate("MainWindow", "I Max (A)", nullptr));
        minVoltageTitle->setText(QCoreApplication::translate("MainWindow", "V Min (V)", nullptr));
        maxVoltageTitle->setText(QCoreApplication::translate("MainWindow", "V Max (V)", nullptr));
        minPowerTitle->setText(QCoreApplication::translate("MainWindow", "P Min (W)", nullptr));
        maxPowerTitle->setText(QCoreApplication::translate("MainWindow", "P Max (W)", nullptr));
        logGroupBox->setTitle(QCoreApplication::translate("MainWindow", "Data Log", nullptr));
        connectionStatusGroupBox->setTitle(QCoreApplication::translate("MainWindow", "Device Status", nullptr));
        connectionStatusLabel->setText(QCoreApplication::translate("MainWindow", "Disconnected", nullptr));
        currentChartGroupBox->setTitle(QCoreApplication::translate("MainWindow", "Current Monitor", nullptr));
        voltageChartGroupBox->setTitle(QCoreApplication::translate("MainWindow", "Voltage Monitor", nullptr));
        powerChartGroupBox->setTitle(QCoreApplication::translate("MainWindow", "Power Monitor", nullptr));
        menuFile->setTitle(QCoreApplication::translate("MainWindow", "File", nullptr));
        menuSettings->setTitle(QCoreApplication::translate("MainWindow", "Settings", nullptr));
        menuHelp->setTitle(QCoreApplication::translate("MainWindow", "Help", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
