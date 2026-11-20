/**
 * @file chartwidget.cpp
 * @brief 自定义图表控件实现文件
 * @author 混分巨兽龙某某
 * @version 1.0
 * @date 2026.04.29
 * 
 * 该文件实现了 ChartWidget 类的所有成员函数，包括：
 * - 构造函数和基础设置函数
 * - 数据管理函数（setData、clear、updateAutoScale）
 * - 绘制事件处理（paintEvent）
 * - 鼠标交互事件处理（wheelEvent、mousePressEvent、mouseMoveEvent、mouseReleaseEvent）
 */

#include "chartwidget.h"
#include <QPainter>
#include <QPen>
#include <QFont>
#include <QWheelEvent>
#include <QMouseEvent>
#include <algorithm>

/**
 * @brief 构造函数
 * @param parent 父窗口指针，默认为 nullptr
 * 
 * 初始化图表控件，设置最小尺寸和鼠标跟踪。
 */
ChartWidget::ChartWidget(QWidget *parent) : QWidget(parent), m_color(Qt::blue),
    m_xMin(0), m_xMax(1), m_yMin(0), m_yMax(1), m_isDragging(false),
    m_hasHoverPoint(false), m_hoverIndex(-1), m_tooltipX(0), m_tooltipY(0)
{
    setMinimumSize(200, 150);
    setMouseTracking(true);
}

/**
 * @brief 设置图表标题
 * @param title 标题文本
 */
void ChartWidget::setTitle(const QString &title)
{
    m_title = title;
}

/**
 * @brief 更新自动缩放范围
 * 
 * 根据当前数据自动计算并设置 X 轴和 Y 轴的显示范围，
 * 并在数据范围内添加 10% 的边距。
 */
void ChartWidget::updateAutoScale()
{
    if (m_data.isEmpty()) {
        m_xMin = 0;
        m_xMax = 1;
        m_yMin = 0;
        m_yMax = 1;
        return;
    }
    
    // 计算 Y 轴范围
    double minY = *std::min_element(m_data.begin(), m_data.end());
    double maxY = *std::max_element(m_data.begin(), m_data.end());
    double rangeY = maxY - minY;
    if (rangeY < 0.0001) rangeY = 1.0;
    m_yMin = minY - rangeY * 0.1;
    m_yMax = maxY + rangeY * 0.1;
    
    // 计算 X 轴范围
    double minX = m_time.isEmpty() ? 0 : m_time.first();
    double maxX = m_time.isEmpty() ? 1 : m_time.last();
    m_xMin = minX;
    m_xMax = maxX;
}

/**
 * @brief 设置图表数据
 * @param data Y轴数据列表
 * @param time X轴时间数据列表
 * 
 * 更新图表数据并保留用户设置的缩放状态：
 * - X轴：当时间范围扩展时自动扩展，否则保持当前范围
 * - Y轴：自动扩展以显示所有数据，但不缩小当前范围
 */
void ChartWidget::setData(const QList<double> &data, const QList<double> &time)
{
    // 保存当前缩放状态
    double oldXMin = m_xMin;
    double oldXMax = m_xMax;
    double oldYMin = m_yMin;
    double oldYMax = m_yMax;
    
    // 获取当前最大时间
    double currentMaxX = m_time.isEmpty() ? 0 : m_time.last();
    
    // 更新数据
    m_data = data;
    m_time = time;
    
    // 处理空数据情况
    if (m_data.isEmpty()) {
        updateAutoScale();
        update();
        return;
    }
    
    // 计算新的数据范围
    double newMinY = *std::min_element(m_data.begin(), m_data.end());
    double newMaxY = *std::max_element(m_data.begin(), m_data.end());
    double newRangeY = newMaxY - newMinY;
    if (newRangeY < 0.0001) newRangeY = 1.0;
    
    double newMaxX = m_time.isEmpty() ? 1 : m_time.last();
    
    // 更新 X 轴范围（仅在时间扩展时）
    if (newMaxX > currentMaxX) {
        double extendAmount = newMaxX - currentMaxX;
        m_xMax += extendAmount;
    } else {
        m_xMin = oldXMin;
        m_xMax = oldXMax;
    }
    
    // 更新 Y 轴范围（自动扩展以显示所有数据）
    double paddingY = newRangeY * 0.1;
    double autoMinY = newMinY - paddingY;
    double autoMaxY = newMaxY + paddingY;
    
    m_yMin = qMin(oldYMin, autoMinY);
    m_yMax = qMax(oldYMax, autoMaxY);
    
    // 触发重绘
    update();
}

/**
 * @brief 设置曲线颜色
 * @param color 曲线颜色
 */
void ChartWidget::setColor(const QColor &color)
{
    m_color = color;
}

/**
 * @brief 设置Y轴标签
 * @param label Y轴标签文本
 */
void ChartWidget::setYLabel(const QString &label)
{
    m_yLabel = label;
}

/**
 * @brief 清除图表数据
 * 
 * 清空所有数据并重置显示范围。
 */
void ChartWidget::clear()
{
    m_data.clear();
    m_time.clear();
    updateAutoScale();
    update();
}

/**
 * @brief 重置视图为自动缩放状态
 * 
 * 将图表视图重置为自动缩放模式，显示所有数据。
 */
void ChartWidget::resetView()
{
    updateAutoScale();
    update();
}

/**
 * @brief 重绘事件处理
 * @param event 重绘事件
 * 
 * 绘制图表的各个组成部分：
 * - 背景
 * - 标题
 * - 网格线
 * - X轴和Y轴刻度
 * - 数据曲线（带渐变和发光效果）
 * - 悬停提示框
 */
void ChartWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 定义边距
    int leftMargin = 85;
    int rightMargin = 30;
    int topMargin = 35;
    int bottomMargin = 35;
    
    // 计算图表区域尺寸
    int chartWidth = width() - leftMargin - rightMargin;
    int chartHeight = height() - topMargin - bottomMargin;
    
    // 绘制背景
    painter.fillRect(rect(), QColor(26, 26, 46));
    
    // 绘制标题
    painter.setPen(QColor(52, 152, 219));
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    int titleWidth = painter.fontMetrics().width(m_title);
    painter.drawText(width() / 2 - titleWidth / 2, 22, m_title);
    
    // 处理空数据情况
    if (m_data.isEmpty()) {
        painter.setPen(QColor(127, 140, 141));
        painter.setFont(QFont("Arial", 10));
        painter.drawText(width() / 2 - 40, height() / 2, "No data");
        return;
    }
    
    // 计算数据范围
    double rangeY = m_yMax - m_yMin;
    if (rangeY < 0.0001) rangeY = 1.0;
    
    double rangeX = m_xMax - m_xMin;
    if (rangeX < 0.0001) rangeX = 1.0;
    
    // 绘制图表边框
    painter.setPen(QColor(44, 62, 80));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(leftMargin, topMargin, chartWidth, chartHeight);
    
    // 绘制 Y 轴网格线和刻度
    painter.setPen(QColor(44, 62, 80));
    for (int i = 0; i <= 5; i++) {
        int y = topMargin + (chartHeight * i) / 5;
        painter.drawLine(leftMargin, y, width() - rightMargin, y);
        
        double value = m_yMax - ((m_yMax - m_yMin) * i) / 5;
        QString label = QString("%1").arg(value, 0, 'f', 4);
        int labelWidth = painter.fontMetrics().width(label);
        painter.drawText(leftMargin - labelWidth - 5, y + 4, label);
    }
    
    // 绘制 X 轴网格线和刻度
    painter.setPen(QColor(44, 62, 80));
    for (int i = 0; i <= 4; i++) {
        int x = leftMargin + (chartWidth * i) / 4;
        painter.drawLine(x, topMargin, x, height() - bottomMargin);
        
        double value = m_xMin + ((m_xMax - m_xMin) * i) / 4;
        QString label = QString("%1").arg(value, 0, 'f', 1);
        int labelWidth = painter.fontMetrics().width(label);
        painter.drawText(x - labelWidth / 2, height() - 10, label);
    }
    
    // 创建渐变画笔
    QLinearGradient gradient(leftMargin, topMargin, leftMargin, height() - bottomMargin);
    gradient.setColorAt(0, m_color.lighter(150));
    gradient.setColorAt(1, m_color.darker(150));
    
    QPen linePen;
    linePen.setBrush(gradient);
    linePen.setWidth(2);
    linePen.setCapStyle(Qt::RoundCap);
    linePen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(linePen);
    
    // 绘制数据曲线
    if (m_data.size() >= 2) {
        QPainterPath path;
        bool pathStarted = false;
        
        for (int i = 0; i < m_data.size(); i++) {
            double t = m_time[i];
            if (t >= m_xMin && t <= m_xMax) {
                double x = leftMargin + ((t - m_xMin) / rangeX) * chartWidth;
                double y = topMargin + ((m_yMax - m_data[i]) / rangeY) * chartHeight;
                
                if (!pathStarted) {
                    path.moveTo(x, y);
                    pathStarted = true;
                } else {
                    path.lineTo(x, y);
                }
            }
        }
        
        // 绘制主曲线
        painter.strokePath(path, linePen);
        
        // 绘制发光效果
        QPen glowPen(m_color.lighter(200), 6);
        glowPen.setCapStyle(Qt::RoundCap);
        glowPen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(glowPen);
        painter.setOpacity(0.3);
        painter.strokePath(path, glowPen);
        painter.setOpacity(1.0);
    }
    
    // 绘制悬停提示
    if (m_hasHoverPoint && m_hoverIndex >= 0) {
        double t = m_time[m_hoverIndex];
        double val = m_data[m_hoverIndex];
        double x = leftMargin + ((t - m_xMin) / rangeX) * chartWidth;
        double y = topMargin + ((m_yMax - val) / rangeY) * chartHeight;
        
        // 绘制悬停点指示器
        painter.setBrush(QColor(255, 255, 255, 20));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(x, y), 8, 8);
        
        painter.setBrush(QColor(255, 255, 255));
        painter.drawEllipse(QPointF(x, y), 4, 4);
        
        // 绘制提示框
        QString tooltip = QString("X: %1 s\nY: %2").arg(t, 0, 'f', 4).arg(val, 0, 'f', 4);
        QRect tooltipRect = painter.fontMetrics().boundingRect(tooltip);
        tooltipRect.adjust(-5, -5, 5, 5);
        
        // 计算提示框位置（避免超出边界）
        int tooltipX = x > width() / 2 ? leftMargin + 5 : width() - rightMargin - tooltipRect.width() - 5;
        int tooltipY = y > height() / 2 ? topMargin + 5 : height() - bottomMargin - tooltipRect.height() - 5;
        
        // 绘制提示框背景
        painter.setBrush(QColor(44, 62, 80));
        painter.setPen(QColor(52, 152, 219));
        painter.drawRect(tooltipX, tooltipY, tooltipRect.width(), tooltipRect.height());
        
        // 绘制提示框文字
        painter.setPen(QColor(255, 255, 255));
        painter.setFont(QFont("Arial", 8));
        painter.drawText(tooltipX + 5, tooltipY + 12, tooltip);
    }
}

/**
 * @brief 滚轮事件处理（缩放）
 * @param event 滚轮事件
 * 
 * 实现以鼠标位置为中心的缩放功能：
 * - 滚轮向上：放大图表
 * - 滚轮向下：缩小图表
 * - 限制缩放范围，避免过度缩放
 */
void ChartWidget::wheelEvent(QWheelEvent *event)
{
    if (m_data.isEmpty()) return;
    
    // 定义边距
    int leftMargin = 85;
    int rightMargin = 30;
    int topMargin = 35;
    int bottomMargin = 35;
    
    int chartWidth = width() - leftMargin - rightMargin;
    int chartHeight = height() - topMargin - bottomMargin;
    
    // 获取鼠标位置
    QPoint mousePos = event->pos();
    double x = mousePos.x();
    double y = mousePos.y();
    
    // 检查鼠标是否在图表区域内
    if (x < leftMargin || x > width() - rightMargin || y < topMargin || y > height() - bottomMargin) {
        return;
    }
    
    // 将屏幕坐标转换为图表坐标
    double chartX = (x - leftMargin) / (double)chartWidth;
    double chartY = (y - topMargin) / (double)chartHeight;
    
    // 将图表坐标转换为数据坐标
    double dataX = m_xMin + chartX * (m_xMax - m_xMin);
    double dataY = m_yMax - chartY * (m_yMax - m_yMin);
    
    // 计算缩放因子（向上放大，向下缩小）
    double zoomFactor = event->delta() > 0 ? 0.7 : 1.3;
    
    // 计算新的范围
    double newRangeX = (m_xMax - m_xMin) * zoomFactor;
    double newRangeY = (m_yMax - m_yMin) * zoomFactor;
    
    // 获取数据边界
    double minTime = m_time.isEmpty() ? 0 : m_time.first();
    double maxTime = m_time.isEmpty() ? 1 : m_time.last();
    double minDataY = *std::min_element(m_data.begin(), m_data.end());
    double maxDataY = *std::max_element(m_data.begin(), m_data.end());
    double dataRangeY = maxDataY - minDataY;
    if (dataRangeY < 0.0001) dataRangeY = 1.0;
    
    // 限制缩放范围
    double minAllowedRangeX = 0.01 * (maxTime - minTime);
    double maxAllowedRangeX = 2.0 * (maxTime - minTime);
    double minAllowedRangeY = 0.01 * dataRangeY;
    double maxAllowedRangeY = 2.0 * dataRangeY;
    
    newRangeX = qBound(minAllowedRangeX, newRangeX, maxAllowedRangeX);
    newRangeY = qBound(minAllowedRangeY, newRangeY, maxAllowedRangeY);
    
    // 计算新的边界（以鼠标位置为中心）
    double ratioX = (dataX - m_xMin) / (m_xMax - m_xMin);
    double ratioY = (dataY - m_yMin) / (m_yMax - m_yMin);
    
    m_xMin = dataX - ratioX * newRangeX;
    m_xMax = dataX + (1 - ratioX) * newRangeX;
    m_yMin = dataY - ratioY * newRangeY;
    m_yMax = dataY + (1 - ratioY) * newRangeY;
    
    // 限制在数据范围内
    m_xMin = qMax(m_xMin, minTime);
    m_xMax = qMin(m_xMax, maxTime);
    m_yMin = qMax(m_yMin, minDataY - dataRangeY * 0.2);
    m_yMax = qMin(m_yMax, maxDataY + dataRangeY * 0.2);
    
    // 触发重绘
    update();
    event->accept();
}

/**
 * @brief 鼠标按下事件处理
 * @param event 鼠标事件
 * 
 * 记录拖动开始状态和鼠标位置。
 */
void ChartWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_lastMousePos = event->pos();
        m_hasHoverPoint = false;
    }
    QWidget::mousePressEvent(event);
}

/**
 * @brief 鼠标移动事件处理（拖动/悬停）
 * @param event 鼠标事件
 * 
 * 处理两种情况：
 * - 拖动状态：根据鼠标位移平移图表
 * - 非拖动状态：检测鼠标悬停在哪个数据点上
 */
void ChartWidget::mouseMoveEvent(QMouseEvent *event)
{
    int leftMargin = 85;
    int rightMargin = 30;
    int topMargin = 35;
    int bottomMargin = 35;
    
    int chartWidth = width() - leftMargin - rightMargin;
    int chartHeight = height() - topMargin - bottomMargin;
    
    if (m_isDragging) {
        // 拖动模式：计算位移并平移图表
        QPoint delta = event->pos() - m_lastMousePos;
        
        // 将屏幕位移转换为数据位移
        double xDelta = delta.x() / (double)chartWidth * (m_xMax - m_xMin);
        double yDelta = delta.y() / (double)chartHeight * (m_yMax - m_yMin);
        
        // 更新数据范围
        m_xMin -= xDelta;
        m_xMax -= xDelta;
        m_yMin += yDelta;
        m_yMax += yDelta;
        
        // 获取数据边界
        double minTime = m_time.isEmpty() ? 0 : m_time.first();
        double maxTime = m_time.isEmpty() ? 1 : m_time.last();
        
        // 限制 X 轴范围
        if (m_xMin < minTime) {
            m_xMax += minTime - m_xMin;
            m_xMin = minTime;
        }
        if (m_xMax > maxTime) {
            m_xMin -= m_xMax - maxTime;
            m_xMax = maxTime;
        }
        
        // 更新鼠标位置
        m_lastMousePos = event->pos();
        update();
    } else {
        // 悬停模式：查找最近的数据点
        QPoint mousePos = event->pos();
        double x = mousePos.x();
        double y = mousePos.y();
        
        // 检查鼠标是否在图表区域内
        if (x >= leftMargin && x <= width() - rightMargin && y >= topMargin && y <= height() - bottomMargin) {
            // 转换为图表坐标
            double chartX = (x - leftMargin) / (double)chartWidth;
            double chartY = (y - topMargin) / (double)chartHeight;
            
            // 转换为数据坐标
            double dataX = m_xMin + chartX * (m_xMax - m_xMin);
            double dataY = m_yMax - chartY * (m_yMax - m_yMin);
            
            // 查找最近的数据点
            double minDist = 1000000;
            int closestIndex = -1;
            
            for (int i = 0; i < m_data.size(); i++) {
                double t = m_time[i];
                if (t >= m_xMin && t <= m_xMax) {
                    double dist = qAbs(t - dataX) + qAbs(m_data[i] - dataY);
                    if (dist < minDist) {
                        minDist = dist;
                        closestIndex = i;
                    }
                }
            }
            
            // 设置悬停状态
            if (closestIndex >= 0 && minDist < 0.5) {
                m_hoverIndex = closestIndex;
                m_hasHoverPoint = true;
            } else {
                m_hasHoverPoint = false;
                m_hoverIndex = -1;
            }
            update();
        } else {
            m_hasHoverPoint = false;
            update();
        }
    }
    QWidget::mouseMoveEvent(event);
}

/**
 * @brief 鼠标释放事件处理
 * @param event 鼠标事件
 * 
 * 结束拖动状态。
 */
void ChartWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}
