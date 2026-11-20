/**
 * @file main.cpp
 * @brief 应用程序入口文件
 * @author 混分巨兽龙某某
 * @version 1.0
 * @date 2026.04.29
 * 
 * iKun Power Analyzer 应用程序的主入口，创建并显示主窗口。
 */

#include "mainwindow.h"
#include <QApplication>

/**
 * @brief 主函数，应用程序入口
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 应用程序退出码
 */
int main(int argc, char *argv[])
{
    // 创建 Qt 应用程序对象
    QApplication a(argc, argv);
    
    // 创建主窗口并显示
    MainWindow w;
    w.show();
    
    // 进入应用程序事件循环
    return a.exec();
}
