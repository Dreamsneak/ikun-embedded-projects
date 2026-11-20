/**
 * @file mainwindow.h
 * @brief 主窗口类头文件
 * @author 混分巨兽龙某某
 * @version 1.0
 * @date 2026.04.29
 * 
 * 该文件定义了 MainWindow 类，是 iKun Power Analyzer 应用程序的主窗口类，
 * 负责串口通信、数据解析、图表显示和用户交互等核心功能。
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QDateTime>
#include <QColor>
#include <QGraphicsDropShadowEffect>
#include "chartwidget.h"

namespace Ui {
class MainWindow;
}

/**
 * @class MainWindow
 * @brief 主窗口类，继承自 QMainWindow
 * 
 * 负责管理应用程序的主界面，包括：
 * - 串口设备连接与数据接收
 * - 实时数据解析与处理
 * - 三通道图表显示（电流、电压、功率）
 * - 用户交互（按钮、菜单操作）
 * - 主题样式管理
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针，默认为 nullptr
     */
    explicit MainWindow(QWidget *parent = 0);
    
    /**
     * @brief 析构函数
     */
    ~MainWindow();

private slots:
    /**
     * @brief 串口数据就绪槽函数
     * 
     * 当串口有数据可读时触发，读取并处理接收到的数据
     */
    void onSerialPortReadyRead();
    
    /**
     * @brief 定时器更新槽函数
     * 
     * 定期更新界面显示和图表数据
     */
    void onTimerUpdate();
    
    /**
     * @brief 清除按钮点击槽函数
     * 
     * 清除所有历史数据和图表显示
     */
    void onClearButtonClicked();
    
    /**
     * @brief 保存按钮点击槽函数
     * 
     * 将历史数据保存为 CSV 文件
     */
    void onSaveButtonClicked();
    
    /**
     * @brief 显示/停止按钮点击槽函数
     * 
     * 控制数据显示的开始和停止
     */
    void onShowButtonClicked();
    
    /**
     * @brief 文件菜单 - 保存数据槽函数
     * 
     * 响应 File -> Save Data 菜单操作
     */
    void on_actionSave_Data_triggered();
    
    /**
     * @brief 文件菜单 - 退出槽函数
     * 
     * 响应 File -> Exit 菜单操作，关闭应用程序
     */
    void on_actionExit_triggered();
    
    /**
     * @brief 设置菜单 - 端口设置槽函数
     * 
     * 响应 Settings -> Port Settings 菜单操作，显示端口配置信息
     */
    void on_actionPort_Settings_triggered();
    
    /**
     * @brief 帮助菜单 - 关于槽函数
     * 
     * 响应 Help -> About 菜单操作，显示关于对话框
     */
    void on_actionAbout_triggered();

private:
    Ui::MainWindow *ui;                  ///< UI 对象指针
    QSerialPort *m_iKunDevice;           ///< 串口设备指针
    QTimer *updateTimer;                 ///< 更新定时器
    QString serialBuffer;                ///< 串口数据缓冲区
    QGraphicsDropShadowEffect *ballShadowEffect;  ///< 连接状态球阴影效果
    bool isShowingData;                  ///< 是否正在显示数据
    
    ChartWidget *currentChart;           ///< 电流图表控件
    ChartWidget *voltageChart;           ///< 电压图表控件
    ChartWidget *powerChart;             ///< 功率图表控件
    
    double currentValue;                 ///< 当前电流值
    double voltageValue;                 ///< 当前电压值
    double powerValue;                   ///< 当前功率值
    double energyValue;                  ///< 累计能量值
    
    double minCurrent, maxCurrent;       ///< 电流最小值和最大值
    double minVoltage, maxVoltage;       ///< 电压最小值和最大值
    double minPower, maxPower;           ///< 功率最小值和最大值
    
    QList<double> currentHistory;        ///< 电流历史数据
    QList<double> voltageHistory;        ///< 电压历史数据
    QList<double> powerHistory;          ///< 功率历史数据
    QList<double> timeHistory;           ///< 时间戳历史数据
    
    int dataPointCount;                  ///< 数据点计数
    qint64 startTime;                    ///< 数据采集起始时间

    /**
     * @brief 检查并连接 iKun 设备
     * @return 成功返回 true，失败返回 false
     */
    bool checkForiKunDevice();
    
    /**
     * @brief 初始化图表控件
     */
    void initCharts();
    
    /**
     * @brief 更新图表数据
     */
    void updateCharts();
    
    /**
     * @brief 解析串口数据
     * @param data 接收到的串口数据字符串
     */
    void parseSerialData(const QString &data);
    
    /**
     * @brief 应用主题样式
     */
    void applyTheme();
    
    /**
     * @brief 格式化数字为指定小数位数
     * @param value 要格式化的数值
     * @param decimals 小数位数，默认为 4
     * @return 格式化后的字符串
     */
    QString formatNumber(double value, int decimals = 4);
};

#endif // MAINWINDOW_H
