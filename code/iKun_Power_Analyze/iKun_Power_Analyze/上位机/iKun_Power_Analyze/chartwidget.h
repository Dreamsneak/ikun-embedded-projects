/**
 * @file chartwidget.h
 * @brief 自定义图表控件头文件
 * @author 混分巨兽龙某某
 * @version 1.0
 * @date 2026.04.29
 * 
 * 该文件定义了 ChartWidget 类，是一个自定义的图表显示控件，
 * 支持数据曲线绘制、缩放、平移和悬停提示等交互功能。
 */

#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QList>
#include <QColor>
#include <QPoint>

/**
 * @class ChartWidget
 * @brief 自定义图表控件，继承自 QWidget
 * 
 * 提供以下功能：
 * - 数据曲线绘制（带渐变和发光效果）
 * - 滚轮缩放（支持鼠标位置为中心）
 * - 鼠标拖动平移
 * - 悬停数据点显示精确坐标
 * - 自动缩放适应数据范围
 */
class ChartWidget : public QWidget
{
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针，默认为 nullptr
     */
    explicit ChartWidget(QWidget *parent = nullptr);
    
    /**
     * @brief 设置图表标题
     * @param title 标题文本
     */
    void setTitle(const QString &title);
    
    /**
     * @brief 设置图表数据
     * @param data Y轴数据列表
     * @param time X轴时间数据列表
     */
    void setData(const QList<double> &data, const QList<double> &time);
    
    /**
     * @brief 设置曲线颜色
     * @param color 曲线颜色
     */
    void setColor(const QColor &color);
    
    /**
     * @brief 设置Y轴标签
     * @param label Y轴标签文本
     */
    void setYLabel(const QString &label);
    
    /**
     * @brief 清除图表数据
     */
    void clear();
    
    /**
     * @brief 重置视图为自动缩放状态
     */
    void resetView();
    
protected:
    /**
     * @brief 重绘事件处理
     * @param event 重绘事件
     */
    void paintEvent(QPaintEvent *event) override;
    
    /**
     * @brief 滚轮事件处理（缩放）
     * @param event 滚轮事件
     */
    void wheelEvent(QWheelEvent *event) override;
    
    /**
     * @brief 鼠标按下事件处理
     * @param event 鼠标事件
     */
    void mousePressEvent(QMouseEvent *event) override;
    
    /**
     * @brief 鼠标移动事件处理（拖动/悬停）
     * @param event 鼠标事件
     */
    void mouseMoveEvent(QMouseEvent *event) override;
    
    /**
     * @brief 鼠标释放事件处理
     * @param event 鼠标事件
     */
    void mouseReleaseEvent(QMouseEvent *event) override;
    
private:
    QString m_title;              ///< 图表标题
    QString m_yLabel;             ///< Y轴标签
    QList<double> m_data;         ///< Y轴数据
    QList<double> m_time;         ///< X轴时间数据
    QColor m_color;               ///< 曲线颜色
    
    double m_xMin, m_xMax;        ///< X轴范围
    double m_yMin, m_yMax;        ///< Y轴范围
    bool m_isDragging;            ///< 是否正在拖动
    QPoint m_lastMousePos;        ///< 上次鼠标位置
    bool m_hasHoverPoint;         ///< 是否有悬停点
    int m_hoverIndex;             ///< 悬停点索引
    int m_tooltipX, m_tooltipY;   ///< 提示框位置
    
    /**
     * @brief 更新自动缩放范围
     */
    void updateAutoScale();
};

#endif // CHARTWIDGET_H
