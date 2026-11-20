/**
 * @file mainwindow.cpp
 * @brief 主窗口类实现文件
 * @author 混分巨兽龙某某
 * @version 1.0
 * @date 2026.04.29
 * 
 * 该文件实现了 MainWindow 类的所有成员函数，包括：
 * - 构造函数和析构函数
 * - 串口通信相关函数
 * - 数据解析和处理函数
 * - 图表更新函数
 * - 用户交互函数（按钮点击、菜单操作）
 * - 主题样式设置函数
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QScrollBar>
#include <QGraphicsDropShadowEffect>

/**
 * @brief 构造函数
 * @param parent 父窗口指针，默认为 nullptr
 * 
 * 初始化主窗口，包括：
 * - UI 设置
 * - 定时器创建
 * - 图表初始化
 * - 主题应用
 * - 信号槽连接
 */
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_iKunDevice(nullptr),
    ballShadowEffect(nullptr),
    isShowingData(true),
    currentValue(0),
    voltageValue(0),
    powerValue(0),
    energyValue(0),
    dataPointCount(0),
    startTime(0)
{
    ui->setupUi(this);
    
    // 创建更新定时器
    updateTimer = new QTimer(this);
    
    // 初始化极值变量
    minCurrent = 99999; maxCurrent = -99999;
    minVoltage = 99999; maxVoltage = -99999;
    minPower = 99999; maxPower = -99999;
    
    // 初始化图表控件
    initCharts();
    
    // 应用主题样式
    applyTheme();
    
    // 设置连接状态球的阴影效果
    ballShadowEffect = new QGraphicsDropShadowEffect(this);
    ballShadowEffect->setColor(QColor("#e74c3c"));
    ballShadowEffect->setBlurRadius(15);
    ballShadowEffect->setOffset(0, 0);
    ui->connectionStatusBall->setGraphicsEffect(ballShadowEffect);
    ui->connectionStatusBall->setStyleSheet("border-radius: 10px; background-color: #e74c3c;");
    
    // 调整主布局比例
    QHBoxLayout *mainLayout = qobject_cast<QHBoxLayout*>(ui->centralWidget->layout());
    if (mainLayout) {
        mainLayout->setStretch(0, 2);
        mainLayout->setStretch(1, 5);
    }
    
    // 连接信号槽
    connect(ui->clearButton, &QPushButton::clicked, this, &MainWindow::onClearButtonClicked);
    connect(ui->saveButton, &QPushButton::clicked, this, &MainWindow::onSaveButtonClicked);
    connect(ui->showButton, &QPushButton::clicked, this, &MainWindow::onShowButtonClicked);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::onTimerUpdate);
    
    // 启动定时器，每100ms更新一次
    updateTimer->start(100);
}

/**
 * @brief 析构函数
 * 
 * 清理资源：
 * - 关闭并释放串口设备
 * - 释放 UI 对象
 */
MainWindow::~MainWindow()
{
    if (m_iKunDevice) {
        if (m_iKunDevice->isOpen()) {
            m_iKunDevice->close();
        }
        m_iKunDevice->deleteLater();
    }
    delete ui;
}

/**
 * @brief 检查并连接 iKun 设备
 * @return 成功返回 true，失败返回 false
 * 
 * 扫描可用串口设备，查找带有特定序列号标识的 iKun 功率分析仪设备，
 * 如果找到则建立串口连接并配置通信参数。
 */
bool MainWindow::checkForiKunDevice()
{
    static bool debugPrinted = false;
    // 只打印一次可用设备列表用于调试
    if (!debugPrinted) {
        debugPrinted = true;
        qDebug() << "=== 可用串口设备列表 ===";
        foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
            qDebug() << "端口名:" << info.portName();
            qDebug() << "  描述:" << info.description();
            qDebug() << "  制造商:" << info.manufacturer();
            qDebug() << "  序列号:" << info.serialNumber();
            qDebug() << "  产品ID:" << info.productIdentifier();
            qDebug() << "  供应商ID:" << info.vendorIdentifier();
            qDebug() << "";
        }
        qDebug() << "=== 设备列表结束 ===";
    }

    // 如果设备已连接，检查是否仍然存在
    if (m_iKunDevice && m_iKunDevice->isOpen()) {
        bool deviceExist = false;

        foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
            if (info.serialNumber().contains("IKUN_POWER_ANALYZE")) {
                deviceExist = true;
                break;
            }
        }

        if (!deviceExist) {
            m_iKunDevice->close();
            m_iKunDevice->deleteLater();
            m_iKunDevice = nullptr;
            return false;
        }

        return true;
    }

    // 尝试连接新设备
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        if (info.serialNumber().contains("IKUN_POWER_ANALYZE")) {
            m_iKunDevice = new QSerialPort(info);

            // 配置串口参数
            m_iKunDevice->setBaudRate(115200);
            m_iKunDevice->setDataBits(QSerialPort::Data8);
            m_iKunDevice->setParity(QSerialPort::NoParity);
            m_iKunDevice->setStopBits(QSerialPort::OneStop);
            m_iKunDevice->setFlowControl(QSerialPort::NoFlowControl);

            if (m_iKunDevice->open(QIODevice::ReadWrite)) {
                qDebug() << "设备连接成功：" << info.portName();
                connect(m_iKunDevice, &QSerialPort::readyRead, this, &MainWindow::onSerialPortReadyRead);
                startTime = QDateTime::currentMSecsSinceEpoch();
                dataPointCount = 0;
                return true;
            } else {
                delete m_iKunDevice;
                m_iKunDevice = nullptr;
            }
        }
    }

    return false;
}

/**
 * @brief 初始化图表控件
 * 
 * 创建三个图表控件（电流、电压、功率）并设置初始属性，
 * 将它们添加到对应的布局中。
 */
void MainWindow::initCharts()
{
    // 创建电流图表
    currentChart = new ChartWidget();
    currentChart->setTitle("Current Monitor");
    currentChart->setColor(QColor(0, 188, 212));
    currentChart->setYLabel("Current (A)");
    ui->currentChartLayout->addWidget(currentChart);
    
    // 创建电压图表
    voltageChart = new ChartWidget();
    voltageChart->setTitle("Voltage Monitor");
    voltageChart->setColor(QColor(76, 175, 80));
    voltageChart->setYLabel("Voltage (V)");
    ui->voltageChartLayout->addWidget(voltageChart);
    
    // 创建功率图表
    powerChart = new ChartWidget();
    powerChart->setTitle("Power Monitor");
    powerChart->setColor(QColor(255, 152, 0));
    powerChart->setYLabel("Power (W)");
    ui->powerChartLayout->addWidget(powerChart);
}

/**
 * @brief 应用主题样式
 * 
 * 设置应用程序的深色主题样式，包括窗口背景、按钮、标签、输入框等控件的样式。
 */
void MainWindow::applyTheme()
{
    QString styleSheet = R"(
        QMainWindow {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                                      stop:0 #1a1a2e, stop:1 #16213e);
        }
        
        QWidget#centralWidget {
            background: transparent;
        }
        
        QGroupBox {
            border: 2px solid #3498db;
            border-radius: 12px;
            margin-top: 10px;
            padding-top: 5px;
            background: rgba(26, 26, 46, 0.8);
        }
        
        QGroupBox::title {
            color: #3498db;
            font-size: 14px;
            font-weight: bold;
            padding-left: 10px;
            padding-right: 10px;
        }
        
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                                      stop:0 #3498db, stop:1 #2980b9);
            border: none;
            border-radius: 8px;
            color: white;
            font-size: 13px;
            font-weight: bold;
            padding: 8px 20px;
            min-width: 80px;
        }
        
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                                      stop:0 #3cb0fd, stop:1 #3498db);
        }
        
        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                                      stop:0 #2980b9, stop:1 #2471a3);
        }
        
        QPushButton:disabled {
            background: #7f8c8d;
        }
        
        QComboBox {
            background: #2c3e50;
            border: 1px solid #3498db;
            border-radius: 8px;
            color: white;
            padding: 6px;
            min-width: 150px;
        }
        
        QComboBox::drop-down {
            background: #3498db;
            border-radius: 0 8px 8px 0;
        }
        
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid white;
            margin-left: -5px;
        }
        
        QLineEdit {
            background: #2c3e50;
            border: 1px solid #3498db;
            border-radius: 8px;
            color: white;
            padding: 8px;
            font-size: 14px;
        }
        
        QLabel {
            color: #ffffff;
            font-weight: 500;
        }
        
        QLabel#valueLabel {
            font-size: 28px;
            font-weight: bold;
            color: #00e5ff;
        }
        
        QLabel#unitLabel {
            font-size: 14px;
            color: #bdc3c7;
        }
        
        QLabel#titleLabel {
            font-size: 16px;
            font-weight: bold;
            color: #3498db;
        }
        
        QTextEdit {
            background: #2c3e50;
            border: 1px solid #3498db;
            border-radius: 8px;
            color: #ecf0f1;
            font-family: "Consolas", "Monaco", monospace;
            font-size: 12px;
        }
        
        QStatusBar {
            background: #1a1a2e;
            color: #ecf0f1;
            border-top: 1px solid #3498db;
        }
    )";
    
    this->setStyleSheet(styleSheet);
}

/**
 * @brief 格式化数字为指定小数位数
 * @param value 要格式化的数值
 * @param decimals 小数位数，默认为 4
 * @return 格式化后的字符串
 */
QString MainWindow::formatNumber(double value, int decimals)
{
    return QString("%1").arg(value, 0, 'f', decimals);
}

/**
 * @brief 串口数据就绪槽函数
 * 
 * 当串口有数据可读时触发，读取所有可用数据并按行解析。
 */
void MainWindow::onSerialPortReadyRead()
{
    // 读取所有可用数据到缓冲区
    serialBuffer += m_iKunDevice->readAll();
    
    // 按行处理数据
    while (serialBuffer.contains('\n')) {
        int newlineIndex = serialBuffer.indexOf('\n');
        QString line = serialBuffer.left(newlineIndex).trimmed();
        serialBuffer.remove(0, newlineIndex + 1);
        
        if (!line.isEmpty()) {
            parseSerialData(line);
        }
    }
}

/**
 * @brief 解析串口数据
 * @param data 接收到的串口数据字符串
 * 
 * 解析格式为 "U: x.xxxx V; I: y.yyyy A; P: z.zzzz W" 的数据字符串，
 * 提取电压、电流、功率值并更新历史数据。
 */
void MainWindow::parseSerialData(const QString &data)
{
    // 如果不在显示状态，忽略数据
    if (!isShowingData) {
        return;
    }
    
    QStringList parts = data.split(";");
    double current = 0, voltage = 0, power = 0;
    bool currentValid = false, voltageValid = false;
    
    // 解析各数据项
    foreach (const QString &part, parts) {
        QString trimmedPart = part.trimmed();
        if (trimmedPart.startsWith("U:")) {
            QString valueStr = trimmedPart.mid(2).trimmed();
            voltage = valueStr.split(" ").first().toDouble(&voltageValid);
        } else if (trimmedPart.startsWith("I:")) {
            QString valueStr = trimmedPart.mid(2).trimmed();
            current = valueStr.split(" ").first().toDouble(&currentValid);
        } else if (trimmedPart.startsWith("P:")) {
            QString valueStr = trimmedPart.mid(2).trimmed();
            power = valueStr.split(" ").first().toDouble();
        }
    }
    
    // 验证数据有效性
    if (currentValid && voltageValid) {
        // 更新当前值
        currentValue = current;
        voltageValue = voltage;
        powerValue = power > 0 ? power : current * voltage;
        
        // 更新极值
        minCurrent = qMin(minCurrent, current);
        maxCurrent = qMax(maxCurrent, current);
        minVoltage = qMin(minVoltage, voltage);
        maxVoltage = qMax(maxVoltage, voltage);
        minPower = qMin(minPower, powerValue);
        maxPower = qMax(maxPower, powerValue);
        
        // 计算相对时间
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        double elapsedTime = (currentTime - startTime) / 1000.0;
        
        // 添加到历史数据
        currentHistory.append(current);
        voltageHistory.append(voltage);
        powerHistory.append(powerValue);
        timeHistory.append(elapsedTime);
        
        dataPointCount++;
        
        // 限制历史数据最大数量
        if (timeHistory.size() > 300) {
            currentHistory.removeFirst();
            voltageHistory.removeFirst();
            powerHistory.removeFirst();
            timeHistory.removeFirst();
        }
        
        // 添加日志
        QString log = QString("[%1] I=%2A, V=%3V, P=%4W")
                     .arg(QTime::currentTime().toString("HH:mm:ss"))
                     .arg(formatNumber(current))
                     .arg(formatNumber(voltage))
                     .arg(formatNumber(powerValue));
        ui->logTextEdit->append(log);
        ui->logTextEdit->verticalScrollBar()->setValue(ui->logTextEdit->verticalScrollBar()->maximum());
    }
}

/**
 * @brief 定时器更新槽函数
 * 
 * 定期更新界面显示，包括：
 * - 更新数值显示
 * - 更新极值显示
 * - 更新能量累计
 * - 更新图表
 * - 更新连接状态
 */
void MainWindow::onTimerUpdate()
{
    // 检查设备连接状态
    bool isConnected = checkForiKunDevice();
    
    // 更新数值显示
    ui->currentValueLabel->setText(formatNumber(currentValue));
    ui->voltageValueLabel->setText(formatNumber(voltageValue));
    ui->powerValueLabel->setText(formatNumber(powerValue));
    ui->energyValueLabel->setText(formatNumber(energyValue));
    
    // 更新极值显示
    ui->minCurrentLabel->setText(formatNumber(minCurrent));
    ui->maxCurrentLabel->setText(formatNumber(maxCurrent));
    ui->minVoltageLabel->setText(formatNumber(minVoltage));
    ui->maxVoltageLabel->setText(formatNumber(maxVoltage));
    ui->minPowerLabel->setText(formatNumber(minPower));
    ui->maxPowerLabel->setText(formatNumber(maxPower));
    
    // 累计能量并更新图表
    if (dataPointCount > 0) {
        energyValue += powerValue / 3600.0 / 10.0;
        updateCharts();
    }
    
    // 更新连接状态显示
    QString status = isConnected ? "Connect" : "Disconnected";
    ui->connectionStatusLabel->setText(status);
    ui->connectionStatusLabel->setStyleSheet(isConnected 
        ? "color: #27ae60; font-size: 14px; font-weight: bold;" 
        : "color: #e74c3c; font-size: 14px; font-weight: bold;");
    
    // 更新连接状态球颜色
    QString ballColor = isConnected ? "#27ae60" : "#e74c3c";
    ui->connectionStatusBall->setStyleSheet(QString("border-radius: 10px; background-color: %1;").arg(ballColor));
    if (ballShadowEffect) {
        ballShadowEffect->setColor(QColor(ballColor));
    }
}

/**
 * @brief 更新图表数据
 * 
 * 将历史数据同步到三个图表控件中。
 */
void MainWindow::updateCharts()
{
    currentChart->setData(currentHistory, timeHistory);
    voltageChart->setData(voltageHistory, timeHistory);
    powerChart->setData(powerHistory, timeHistory);
}

/**
 * @brief 清除按钮点击槽函数
 * 
 * 清除所有历史数据和图表显示，重置统计变量。
 */
void MainWindow::onClearButtonClicked()
{
    // 清除日志
    ui->logTextEdit->clear();
    
    // 清除历史数据
    currentHistory.clear();
    voltageHistory.clear();
    powerHistory.clear();
    timeHistory.clear();
    dataPointCount = 0;
    startTime = QDateTime::currentMSecsSinceEpoch();
    energyValue = 0;
    
    // 重置极值
    minCurrent = 99999; maxCurrent = -99999;
    minVoltage = 99999; maxVoltage = -99999;
    minPower = 99999; maxPower = -99999;
    
    // 清除图表
    currentChart->clear();
    voltageChart->clear();
    powerChart->clear();
    
    // 显示状态消息
    ui->statusBar->showMessage("Data cleared", 3000);
}

/**
 * @brief 保存按钮点击槽函数
 * 
 * 将历史数据保存为 CSV 文件，包含时间、电流、电压、功率四列。
 */
void MainWindow::onSaveButtonClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save Data", 
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_power_data.csv", 
        "CSV Files (*.csv)");
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            // 写入表头
            out << "Time(s),Current(A),Voltage(V),Power(W)\n";
            
            // 写入数据
            for (int i = 0; i < timeHistory.size(); i++) {
                out << formatNumber(timeHistory[i]) << ","
                    << formatNumber(currentHistory[i]) << ","
                    << formatNumber(voltageHistory[i]) << ","
                    << formatNumber(powerHistory[i]) << "\n";
            }
            
            file.close();
            ui->statusBar->showMessage("Data saved successfully", 3000);
        } else {
            QMessageBox::critical(this, "Error", "Failed to save file!");
        }
    }
}

/**
 * @brief 显示/停止按钮点击槽函数
 * 
 * 控制数据显示的开始和停止，切换按钮文字和颜色。
 */
void MainWindow::onShowButtonClicked()
{
    isShowingData = !isShowingData;
    
    if (isShowingData) {
        ui->showButton->setText("Stop");
        ui->showButton->setStyleSheet("background-color: #e74c3c; color: white;");
    } else {
        ui->showButton->setText("Run");
        ui->showButton->setStyleSheet("background-color: #27ae60; color: white;");
    }
}

/**
 * @brief 文件菜单 - 保存数据槽函数
 * 
 * 响应 File -> Save Data 菜单操作，调用保存功能。
 */
void MainWindow::on_actionSave_Data_triggered()
{
    onSaveButtonClicked();
}

/**
 * @brief 文件菜单 - 退出槽函数
 * 
 * 响应 File -> Exit 菜单操作，关闭应用程序。
 */
void MainWindow::on_actionExit_triggered()
{
    close();
}

/**
 * @brief 设置菜单 - 端口设置槽函数
 * 
 * 响应 Settings -> Port Settings 菜单操作，显示端口配置信息对话框。
 */
void MainWindow::on_actionPort_Settings_triggered()
{
    QString portInfo;
    if (m_iKunDevice && m_iKunDevice->isOpen()) {
        portInfo = QString(
            "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">"
            "<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">"
            "p { white-space: pre-wrap; color: #000000; font-size: 14px; }"
            "strong { color: #2c3e50; }"
            "</style></head><body style=\"font-family: Arial;\">"
            "<p><strong>Port:</strong> %1</p>"
            "<p><strong>Baud Rate:</strong> %2</p>"
            "<p><strong>Data Bits:</strong> 8</p>"
            "<p><strong>Parity:</strong> None</p>"
            "<p><strong>Stop Bits:</strong> 1</p>"
            "<p><strong>Flow Control:</strong> None</p>"
            "</body></html>"
        ).arg(m_iKunDevice->portName()).arg(m_iKunDevice->baudRate());
    } else {
        portInfo = QString(
            "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">"
            "<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">"
            "p { color: #000000; font-size: 14px; }"
            "</style></head><body style=\"font-family: Arial;\">"
            "<p>No device connected</p>"
            "</body></html>"
        );
    }
    
    QMessageBox::information(this, "Port Settings", portInfo);
}

/**
 * @brief 帮助菜单 - 关于槽函数
 * 
 * 响应 Help -> About 菜单操作，显示关于对话框，包含作者、版本、日期等信息。
 */
void MainWindow::on_actionAbout_triggered()
{
    QString aboutText = QString(
        "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">"
        "<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">"
        "p, li { white-space: pre-wrap; color: #000000; }"
        "h2 { color: #2c3e50; font-weight: bold; }"
        "strong { color: #2c3e50; }"
        "</style></head><body style=\"font-family: Arial;\">"
        "<h2>iKun Power Analyzer</h2>"
        "<p><strong>Author:</strong> 混分巨兽龙某某</p>"
        "<p><strong>Version:</strong> ikun_Power_Analyze_v1.0</p>"
        "<p><strong>Date:</strong> 2026.04.29</p>"
        "<p><br/><span style=\"color: #34495e;\">Power analysis tool for monitoring and recording electrical parameters.</span></p>"
        "</body></html>"
    );
    
    QMessageBox::about(this, "About iKun Power Analyzer", aboutText);
}
